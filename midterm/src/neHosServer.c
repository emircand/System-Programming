#include "neHosLib.h"


#define BUF_SIZE 1024
#define MAX_FILES 100
#define SHM_NAME "/shm"
#define SEM_NAME "/sem"
FILE* log_file = NULL;
pid_t child_pids[BUF_SIZE];
int child_count = 0;

void sig_handler(int signum);
void queue_init(struct queue* q, int capacity);
void enqueue(struct queue* q, struct request* req);
struct request* dequeue(struct queue* q);
char** getFilesInDirectory(const char* dirname); 
void err_exit(const char *err);
void handle_client_request(struct request* req, pid_t* child_pids, int* child_count, int server_fifo_fd, int client_id);



int main(int argc, char* argv[]) {
    int server_fifo_fd, client_fifo_fd, num_read, num_write;
    int server_fd, dummy_fd, client_fd, client_id, client_max;
    char buf[BUF_SIZE];
    int shm_fd;
    void* shm_ptr;
    sem_t sem;
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
    struct queue request_queue;
    queue_init(&request_queue, max_clients);
    

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
        // Handle the client's request
        ++client_id;
        handle_client_request(req, child_pids, &child_count, server_fifo_fd, client_id);
    }

    // Clean up
    close(server_fifo_fd);
    unlink(server_fifo);
    fclose(log_file);
    return 0;
}

void handle_client_request(struct request* req, pid_t* child_pids, int* child_count, int server_fifo_fd, int client_id) {
    // Fork a new process for each client
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: handle the client's request

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
        printf(">> Child PID: %d connected as client%d\n", req->pid, client_id);

        char response[BUF_SIZE] = "Hello from server";

        if (write(client_fd, response, sizeof(response)) != sizeof(response)) {
            perror("write");
        }

        // Close the client FIFO
        close(client_fd);
        close(server_fifo_fd);

        printf(">> client%d disconnected\n", client_id);

        // Free the request
        free(req);

        // Exit the child process
        exit(EXIT_SUCCESS);
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

void queue_init(struct queue* q, int capacity) {
    q->capacity = capacity;
    q->front = q->size = 0;
    q->rear = capacity - 1;
}

void enqueue(struct queue* q, struct request* req) {
    if (q->size == q->capacity) {
        fprintf(stderr, "Queue is full\n");
        return;
    }
    q->rear = (q->rear + 1) % q->capacity;
    q->elements[q->rear] = req;
    q->size++;
}

struct request* dequeue(struct queue* q) {
    if (q->size == 0) {
        fprintf(stderr, "Queue is empty\n");
        return NULL;
    }
    struct request* req = q->elements[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return req;
}

void err_exit(const char *err) 
{
    perror(err);
    exit(EXIT_FAILURE);
}