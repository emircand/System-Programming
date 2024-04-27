#include "neHosLib.h"

#define BUF_SIZE 1024
#define MAX_FILES 100
#define SHM_NAME "/shm"
#define SEM_NAME "/sem"
FILE* log_file = NULL;
struct Queue* child_pids;

int child_count = 0;
int max_clients, server_fifo_fd, server_fd;
sem_t sem;

int main(int argc, char* argv[]) {
    pid_t last_pid;
    int server_fifo_fd, client_fifo_fd, num_read, num_write;
    int server_fd, dummy_fd, client_fd;
    char buf[BUF_SIZE];
    struct sigaction sa;
    
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_handler = &sig_handler;
    if (sigemptyset(&sa.sa_mask) == -1 || 
        sigaction(SIGINT, &sa, NULL) == -1 || 
        sigaction(SIGTERM, &sa, NULL) == -1)
        perror("sa_handle");


    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dirname> <max. #ofClients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* dir_name = argv[1];
    max_clients = atoi(argv[2]);
    child_pids = createQueue(max_clients);
    

    /* Create the server folder if it doesn't exist */
    if ((mkdir(dir_name, S_IRWXU | S_IWUSR | S_IRUSR | S_IXUSR | S_IWGRP | S_IRGRP)) == -1 && errno != EEXIST)
        err_exit("mkdir");

    /* Open server folder */
    if ((dir_name = opendir(argv[1])) == NULL)
        err_exit("opendir");


    // Create a log file
    FILE* log_file = fopen("log.txt", "w");
    if (log_file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Print the server's PID
    printf(">> Server Started PID: %d\n", getpid());
    printf(">> Waiting for clients...\n");

    char server_fifo[BUF_SIZE];

    sprintf(server_fifo, SERVER_FIFO_TEMP, getpid());

    // Create the server FIFO
    if(mkfifo(server_fifo, 0666) == -1){
        perror("mkfifo");
    }

    // Open the server FIFO
    server_fifo_fd = open(server_fifo, O_RDONLY);
    if (server_fifo_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Open the dummy FIFO for writing to avoid EOF
    dummy_fd = open(server_fifo, O_WRONLY);
    if (dummy_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    printf(">> Server FIFO opened\n");
    last_pid = 0;
    // Handle the client's requests
    // Declare and initialize the mutex
    // pthread_mutex_t lock;
    // pthread_mutex_init(&lock, NULL);

    while (1) {
        struct request* req = malloc(sizeof(struct request));
        if (!req) {
            perror("malloc");
            continue;
        }

        ssize_t numRead = read(server_fifo_fd, req, sizeof(struct request));
        if (numRead <= 0) {
            if (numRead == 0) {
                fprintf(stderr, "Unexpected EOF\n");
            } else {
                perror("read");
            }
            free(req);
            break;
        }

        // Lock the mutex before checking the number of connected clients
        // pthread_mutex_lock(&lock);

        // Enqueue the incoming request
        enqueue(child_pids, req);

        // Dequeue a request
        req = dequeue(child_pids);

        // Handle connection request
        if (req->type == CONNECT) {
            if (child_count + 1 > max_clients) {
                send_connect_response(req->pid, true);
                // pthread_mutex_unlock(&lock);
                continue;
            }
            req->client_number = child_count + 1;
            printf(">> Child PID: %d connected as client%d\n", req->pid, req->client_number);
            send_connect_response(req->pid, false);
            child_count++; // Increment only after successful connection
            // pthread_mutex_unlock(&lock);
            continue;
        }

        // Unlock the mutex after handling the connection request
        // pthread_mutex_unlock(&lock);

        // Handle command request
        if (req->type == COMMAND) {
            handle_client_request(req, child_pids, &child_count, server_fifo_fd);
            last_pid = req->pid;  // Track the last PID for any necessary action
        }
    }

    // Destroy the mutex
    // pthread_mutex_destroy(&lock);

    // Clean up
    close(server_fifo_fd);
    unlink(server_fifo);
    fclose(log_file);
    return 0;
}

void send_connect_response(pid_t client_pid, bool wait) {
    struct response resp;
    if(wait){
        resp.status = RESP_ERROR;
        snprintf(resp.data, BUF_SIZE, "Server full");
    } else {
        resp.status = RESP_CONNECT;
        snprintf(resp.data, BUF_SIZE, "Connect");
    }
    
    char client_fifo[256];
    snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMP, client_pid);
    int client_fd = open(client_fifo, O_WRONLY);
    write(client_fd, &resp, sizeof(resp));
    close(client_fd);
}

void handle_command(char* response, const char* command) {
    if (strcmp(command, "help") == 0) {
        strcpy(response, "help, list, readF, writeT, upload, download, archServer, quit, killServer");
    } else if (strcmp(command, "list") == 0) {
        strcpy(response, "Displaying the list of files in Servers directory...");
    } else if (strncmp(command, "readF", 5) == 0) {
        strcpy(response, "Displaying the # line of the file...");
    } else if (strncmp(command, "writeT", 6) == 0) {
        strcpy(response, "Writing to the #th line the file...");
    } else if (strncmp(command, "upload", 6) == 0) {
        strcpy(response, "Uploading the file...");
    } else if (strncmp(command, "download", 8) == 0) {
        strcpy(response, "Downloading the file...");
    } else if (strncmp(command, "archServer", 10) == 0) {
        strcpy(response, "Archiving the server files...");
    } else if (strcmp(command, "killServer") == 0) {
        strcpy(response, "Killing the server...");
    } else if (strcmp(command, "quit") == 0) {
        strcpy(response, "Quitting...");
    } else {
        strcpy(response, "Invalid command");
    }
}

void handle_client_request(struct request* req, pid_t* child_pids, int* child_count, int server_fifo_fd) {
    if(sem_init(&sem, 1, 1) == -1){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    // Fork a new process for each client
    pid_t pid = fork();
    if (pid == 0) {

        
        // Wait for the semaphore
        sem_wait(&sem);
        // Create the client FIFO name
        char client_fifo[BUF_SIZE];
        snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMP, req->pid);

        // Open the client FIFO for writing
        int client_fd = open(client_fifo, O_WRONLY);
        if (client_fd == -1) {
            perror("open");
            free(req);
            return;
        }

        // Handle the client's request
        struct response resp;
        handle_command(resp.data, req->data);
        resp.status = RESP_OK;

        if (write(client_fd, &resp, sizeof(resp)) != sizeof(resp)) {
            perror("write");
        }

        // Close the client FIFO
        close(client_fd);

        if (strcmp(resp.data, "Quitting...") == 0) {
            // Exit the child process
            printf(">> client%d disconnected\n", req->client_number);
            unlink(client_fifo);
            // pthread_mutex_lock(&lock);
            (*child_count)--;
            // pthread_mutex_unlock(&lock);
            // Exit the child process
            exit(EXIT_SUCCESS);
        }      
        sem_post(&sem);
    } else if (pid > 0) {
        // Parent process: do nothing, wait for the next client
        int status;
        waitpid(pid, &status, WNOHANG);
        free(req);
    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}


void sig_handler(int signum) {
    int pid;
    int status;

    switch (signum) {
        case SIGTERM:
        case SIGQUIT:
        case SIGINT:
            fprintf(stderr, "\nHandling signal: %d\n", signum);    
            break;
        case SIGCHLD:
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                sem_wait(&sem);
                for (int i = 0; i < child_count; i++) {
                    if (child_pids->array[i]->pid == pid) {
                        kill(child_pids->array[i]->pid, SIGTERM);
                        printf("Child PID %d terminated.\n", pid);
                        child_count--; // Decrement child_count when a child process terminates
                        break;
                    }
                }
                sem_post(&sem);
            }
        default:
            fprintf(stderr, "\nHandling signal: %d\n", signum);    
            break;
    }

    // Close server FIFO
    close(server_fifo_fd);
    // unlink(server_fifo);

    // Ensure the log file is created properly
    if (log_file != NULL) {
        fclose(log_file);
    }

    printf(">> bye\n");
    // Exit
    exit(EXIT_SUCCESS);
}

void err_exit(const char *err) 
{
    perror(err);
    exit(EXIT_FAILURE);
}

struct Queue* createQueue(unsigned capacity) {
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (struct request**) malloc(queue->capacity * sizeof(struct request*));
    return queue;
}

int isFull(struct Queue* queue) {
    return (queue->size == queue->capacity);
}

int isEmpty(struct Queue* queue) {
    return (queue->size == 0);
}

void enqueue(struct Queue* queue, struct request* item) {
    if (isFull(queue))
        return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

struct request* dequeue(struct Queue* queue) {
    if (isEmpty(queue))
        return NULL;
    struct request* item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}