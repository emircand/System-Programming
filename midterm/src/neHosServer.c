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



int main(int argc, char* argv[]) {
    int server_fifo_fd, client_fifo_fd;
    char buf[BUF_SIZE];
    int shm_fd;
    void* shm_ptr;
    sem_t* sem;
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
    

    // Change to the specified directory, creating it if it doesn't exist
    mkdir(dir_name, 0777);
    if (chdir(dir_name) == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

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

    printf(">> Server FIFO created %s\n", server_fifo);

    // Open the server FIFO
    server_fifo_fd = open(server_fifo, O_RDONLY);
    if (server_fifo_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    printf(">> Server FIFO opened\n");

    // Handle the client's requests
    while (1) { // Changed from 'while (i < 2)'
        // Read the client's request from the server FIFO
        struct request* req = malloc(sizeof(struct request));
        ssize_t numRead = read(server_fifo_fd, req, sizeof(struct request));
        if (numRead == -1) {
            perror("read");
            free(req);
            continue;
        } else if (numRead != sizeof(struct request)) {
            fprintf(stderr, "Partial/over read. Expected %ld, got %ld\n", sizeof(struct request), numRead);
            free(req);
            break;
        }
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
                continue;
            }
            printf(">> Child PID: %d connected as client\n", req->pid);
            printf("DELETE >> Request data: %s\n", req->data);

            
            char response[BUF_SIZE] = "Hello from server";

            if (write(client_fd, response, sizeof(response)) != sizeof(response)) {
                perror("write");
            }

            // Close the client FIFO
            close(client_fd);

            // Free the request
            free(req);

            // Exit the child process
            exit(EXIT_SUCCESS);
        } else if (pid > 0) {
            // Parent process: do nothing, wait for the next client
            int status;
            child_pids[child_count++] = pid;
            waitpid(pid, &status, WNOHANG);
            printf(">> Waiting for clients...\n");
            
        } else {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
    }

    // Clean up
    close(server_fifo_fd);
    unlink(server_fifo);
    // munmap(shm_ptr, BUF_SIZE);
    // close(shm_fd);
    // shm_unlink(SHM_NAME);
    // sem_close(sem);
    // sem_unlink(SEM_NAME);

    
    return 0;
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

char** getFilesInDirectory(const char* dirname) {
    DIR* directory;
    struct dirent* entry;
    char data[512], *tmp;

    directory = opendir(dirname);
    if (directory == NULL) {
        perror("Unable to open directory");
        fprintf(stderr, "Error: %s\n", dirname);
        exit(EXIT_FAILURE);
    }

    /* Reset the position of the directory stream directory to the beginning of the directory */
    rewinddir(directory);

    /* Take guard for the empty directory */
    data[0] = '\0';
    tmp = data;
    while ((entry = readdir(directory)) != NULL) {
        /* Skip the hidden files */
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            sprintf(tmp, "%s\n", entry->d_name);
            tmp += strlen(tmp);
        }
    }

    closedir(directory);

    /* Remove the last new line */
    data[strlen(data) - 1] = '\0';

    /* Convert data to char** for compatibility with existing code */
    char** files = malloc(2 * sizeof(char*)); // Only two elements - one for the data and one for the NULL terminator
    if (!files) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    files[0] = strdup(data); // Duplicate the data string
    files[1] = NULL; // Null-terminate the array

    return files;
}