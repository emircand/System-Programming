#include "neHosLib.h"

#define BUF_SIZE 1024
#define MAX_FILES 100
#define SHM_NAME "/shm"
#define SEM_NAME "/sem"
FILE* log_file = NULL;
pid_t child_pids[BUF_SIZE];
int child_count = 0;

int main(int argc, char* argv[]) {
    pid_t last_pid;
    int server_fifo_fd, client_fifo_fd, num_read, num_write;
    int server_fd, dummy_fd, client_fd, client_id, client_max;
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
    int max_clients = atoi(argv[2]);
    struct Queue* queue = createQueue(max_clients);
    

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

    client_id = 0;
    last_pid = 0;
    // Handle the client's requests
    while (1) {
        // Read the client's request from the server FIFO
        struct request* req = malloc(sizeof(struct request));
        ssize_t numRead = read(server_fifo_fd, req, sizeof(struct request));
        if (numRead == -1) {
            perror("read");
            free(req);
            continue;
        } else if (numRead == 0) {
            // No data read, continue waiting for the next client
            free(req);
            continue;
        }
        enqueue(queue, req);
        while (!isEmpty(queue)) {
            struct request* req = dequeue(queue);
            // Handle the client's request
            if(req->pid != last_pid){
                ++client_id;
                printf(">> Child PID: %d connected as client%d\n", req->pid, client_id);
            }
            last_pid = req->pid;
            handle_client_request(req, child_pids, &child_count, server_fifo_fd, client_id);
        }
    }

    // Clean up
    close(server_fifo_fd);
    unlink(server_fifo);
    fclose(log_file);
    return 0;
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

void handle_client_request(struct request* req, pid_t* child_pids, int* child_count, int server_fifo_fd, int client_id) {
    // // Create a shared memory object
    // int shm_fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0666);
    // if (shm_fd == -1) {
    //     perror("shm_open");
    //     exit(EXIT_FAILURE);
    // }

    // // Set the size of the shared memory object
    // if (ftruncate(shm_fd, sizeof(sem_t)) == -1) {
    //     perror("ftruncate");
    //     exit(EXIT_FAILURE);
    // }

    // // Map the shared memory object into our address space
    // sem_t* semaphore = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    // if (semaphore == MAP_FAILED) {
    //     perror("mmap");
    //     exit(EXIT_FAILURE);
    // }

    // Initialize the semaphore
    // if (sem_init(semaphore, 1, 1) == -1) {
    //     perror("sem_init");
    //     exit(EXIT_FAILURE);
    // }

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
        char response[BUF_SIZE];
        handle_command(response, req->data);

        if (write(client_fd, response, sizeof(response)) != sizeof(response)) {
            perror("write");
        }

        // Close the client FIFO
        close(client_fd);

        if (strcmp(response, "Quitting...") == 0) {
            // Exit the child process
            printf(">> client%d disconnected\n", client_id);
            // Free the request
            free(req);
            // Exit the child process
            exit(EXIT_SUCCESS);
        }        

        sem_post(&sem);
    } else if (pid > 0) {
        // Parent process: do nothing, wait for the next client
        int status;
        child_pids[*child_count] = pid;
        (*child_count)++;
        waitpid(pid, &status, WNOHANG);
    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // // Unmap the shared memory object
    // if (munmap(semaphore, sizeof(sem_t)) == -1) {
    //     perror("munmap");
    //     exit(EXIT_FAILURE);
    // }

    // // Close the shared memory object
    // if (close(shm_fd) == -1) {
    //     perror("close");
    //     exit(EXIT_FAILURE);
    // }

    // // Remove the shared memory object
    // if (shm_unlink("/my_shm") == -1) {
    //     perror("shm_unlink");
    //     exit(EXIT_FAILURE);
    // }
}


void sig_handler(int signum) {
    int pid;
    int status;

    /* Using system calls is not recommended in signal handler but for sake of example it's preferred that way */
    switch (signum) {
        case SIGTERM:
            fprintf(stderr, "\nHandling SIGTERM\n");    
            break;
        case SIGQUIT:
            fprintf(stderr, "\nHandling SIGQUIT\n");    
            break;
        case SIGINT:
            fprintf(stderr, "\nHandling SIGINT\n");    
            break;
        // case SIGCHLD:
        //     while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        //         if (WIFEXITED(status)) {
        //             printf("(Child PID: %d) exited with status %d\n", pid, WEXITSTATUS(status));
        //         } else if (WIFSIGNALED(status)) {
        //             printf("(Child PID: %d) killed by signal %d\n", pid, WTERMSIG(status));
        //         }
        //     }
        //     return;
        default:
            fprintf(stderr, "\nHandling SIGNUM: %d\n", signum);    
            break;
    }

    // Send kill signals to the child processes
    for (int i = 0; i < child_count; i++) {
        kill(child_pids[i], SIGTERM);
    }

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