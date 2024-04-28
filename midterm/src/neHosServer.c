#include "neHosLib.h"

#define BUF_SIZE 1024
#define MAX_FILES 100
#define SHM_NAME "/shm"
#define SEM_NAME "/sem"
#define SHM_SIZE 4096
FILE* log_file = NULL;
struct Queue* child_pids;

int child_count = 0;
int max_clients;
int server_fifo_fd, client_fifo_fd, num_read, num_write;
int server_fd, dummy_fd, client_fd, log_fd;
sem_t sem;
int shm_fd;
SharedData* shared_data;


int main(int argc, char* argv[]) {
    pid_t last_pid;
    
    char buf[BUF_SIZE];
    
    setup_signals();


    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dirname> <max. #ofClients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* dir_name = argv[1];
    max_clients = atoi(argv[2]);
    child_pids = createQueue(max_clients);

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // Resize the shared memory object
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // Memory map the shared object
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Initialize the semaphore
    if (sem_init(&shared_data->sem, 1, 1) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    
    /* Create the server folder if it doesn't exist */
    if ((mkdir(dir_name, S_IRWXU | S_IWUSR | S_IRUSR | S_IXUSR | S_IWGRP | S_IRGRP)) == -1 && errno != EEXIST)
        err_exit("mkdir");

    /* Open server folder */
    if ((dir_name = opendir(argv[1])) == NULL)
        err_exit("opendir");

    // Create a log file
    char log_path[BUF_SIZE];
    strcpy(log_path, argv[1]);
    strcat(log_path, "/log.txt");

    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("open");
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
    // Initialize child_count
    shared_data->child_count = 0;

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
                sleep(1);
                continue;
            } else {
                perror("read");
            }
            free(req);
            break;
        }

        // Enqueue the incoming request
        enqueue(child_pids, req);

        // Dequeue a request
        req = dequeue(child_pids);

        // Handle connection request
        if (req->type == CONNECT || req->type == TRY_CONNECT) {
            sem_wait(&shared_data->sem);
            if (shared_data->child_count >= max_clients) {
                printf(">> Connection request PID %d… Queue FULL \n", req->pid);
                send_connect_response(req->pid, true);
                sem_post(&shared_data->sem);
                free(req); // Ensure the request is freed if not processed
                continue;
            }
            req->client_number = ++(shared_data->child_count);
            
            sem_post(&shared_data->sem);
            printf(">> Child PID: %d connected as client%d\n", req->pid, req->client_number);
            char message[BUF_SIZE];
            snprintf(message, BUF_SIZE, "Client PID: %d connected as client%d", req->pid, req->client_number);
            write_log(message);
            send_connect_response(req->pid, false);
            free(req); // Free the request after handling
            continue;
        }


        // Handle command request
        if (req->type == COMMAND) {
            handle_client_request(req, server_fifo_fd);
            last_pid = req->pid;  // Track the last PID for any necessary action
        }
    }

    // Clean up
    close(server_fifo_fd);
    unlink(server_fifo);
    fclose(log_file);
    // Clean up resources
    return 0;
}


void write_log(const char* message) {
    char buf[BUF_SIZE];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buf, BUF_SIZE, "%Y-%m-%d %H:%M:%S", tm_info);
    dprintf(log_fd, "[%s] %s\n", buf, message);
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

void handle_client_request(struct request* req,  int server_fifo_fd) {
    // Fork a new process for each client
    pid_t pid = fork();
    if (pid == 0) {  // Child process
        // Wait for the semaphore to ensure exclusive access
        sem_wait(&shared_data->sem);
        
        // Create the client FIFO name and open it
        char client_fifo[BUF_SIZE];
        snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMP, req->pid);
        int client_fd = open(client_fifo, O_WRONLY);
        if (client_fd == -1) {
            perror("open");
            sem_post(&shared_data->sem);  // Release semaphore before exiting on failure
            free(req); // Free the request memory
            exit(EXIT_FAILURE);
        }

        // Handle the client's request
        struct response resp;
        handle_command(resp.data, req->data);
        resp.status = RESP_OK;

        if (write(client_fd, &resp, sizeof(resp)) != sizeof(resp)) {
            perror("write");
        }

        close(client_fd);  // Close the FIFO

        // Check if client sent a "Quitting..." command
        if (strcmp(resp.data, "Quitting...") == 0) {
            
            char message[BUF_SIZE];
            snprintf(message, BUF_SIZE, "Child %d - Client%d disconnected", req->pid, shared_data->child_count);
            write_log(message);
            printf("%s\n", message);
            shared_data->child_count--;  // Decrement child count
            sem_post(&shared_data->sem);  // Release semaphore before exiting
            free(req); // Free the request memory
            exit(EXIT_SUCCESS);
        }

        sem_post(&shared_data->sem);  // Release semaphore if not exiting
        free(req);  // Free the request memory on exit of command handling
        exit(EXIT_SUCCESS);
    } else if (pid > 0) {  // Parent process
        int status;
        waitpid(pid, &status, WNOHANG);  // Non-blocking wait
        free(req);  // Free the request memory
    } else {
        perror("fork");
        free(req); // Ensure the request is freed on fork failure
        exit(EXIT_FAILURE);
    }
}

void setup_signals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));  // Initialize the structure to zero
    sa.sa_handler = sig_handler; // Pointer to signal handler function
    sigemptyset(&sa.sa_mask);    // Initialize the signal mask
    sa.sa_flags = SA_RESTART;    // To handle interrupted system calls

    // Set up signal handlers for SIGINT, SIGTERM, SIGQUIT, and SIGCHLD:
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGQUIT, &sa, NULL) == -1 ||
        sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Failed to set signal handlers");
        exit(EXIT_FAILURE);
    }
}

void cleanup() {
    // Close and unlink server FIFO
    if (server_fifo_fd >= 0) close(server_fifo_fd);
    if (dummy_fd >= 0) close(dummy_fd);

    char buf[BUF_SIZE];
    sprintf(buf, SERVER_FIFO_TEMP, getpid());
    unlink(buf);

    // Close log file
    close(log_fd);

    // Unmap and unlink shared memory
    munmap(shared_data, sizeof(SharedData));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    // Destroy semaphore
    sem_destroy(&shared_data->sem);

    printf("Cleanup complete, server shutting down.\n");
    exit(EXIT_SUCCESS);  // Ensure the process exits successfully
}

void sig_handler(int signum) {
    // Depending on the signal type, perform appropriate actions:
    write_log("Server Shutting Down");
    switch (signum) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            // Handle graceful shutdown:
            cleanup();
            break;
        case SIGCHLD:
            // Reap zombie processes and adjust child count:
            while (waitpid(-1, NULL, WNOHANG) > 0) {
                // sem_wait(&shared_data->sem);
                // shared_data->child_count--; // Safely decrement child count
                // sem_post(&shared_data->sem);
            }
            break;
        default:
            fprintf(stderr, "Unhandled signal: %d\n", signum);
    }
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
    if (isFull(queue)) {
        // Optionally handle the error or log it
        return;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size++;
}

struct request* dequeue(struct Queue* queue) {
    if (isEmpty(queue)) {
        return NULL;
    }
    struct request* item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return item;
}
