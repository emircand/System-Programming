#include "neHosLib.h"
#include "queue.h"


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
int server_fd, dummy_fd, client_fd, log_fd, dir_fd;
DIR* dir;

sem_t file_sem, sem;
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

    init_shared_data();
    
    /* Create the server folder if it doesn't exist */
    if ((mkdir(dir_name, S_IRWXU | S_IWUSR | S_IRUSR | S_IXUSR | S_IWGRP | S_IRGRP)) == -1 && errno != EEXIST)
        err_exit("mkdir");

    /* Open server folder */
    if ((dir = opendir(dir_name)) == NULL)
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
            // Server side: Handling a connection request
            sem_wait(&shared_data->sem);
            if (shared_data->child_count < max_clients) {
                req->client_number = ++(shared_data->child_count);
                sem_post(&shared_data->sem);
                printf(">> Child PID: %d connected as client%d\n", req->pid, req->client_number);
                char message[BUF_SIZE];
                snprintf(message, BUF_SIZE, "Child %d - Client%d connected", req->pid, req->client_number);
                write_log(message);
                send_connect_response(req->pid, false);  // Send success response
            } else {
                sem_post(&shared_data->sem);
                printf(">> Connection request PID %d… Queue FULL \n", req->pid);
                send_connect_response(req->pid, true);  // Send failure response
            }
        }

        // Handle command request
        if (req->type == COMMAND) {
            handle_client_request(req, server_fifo_fd, dir);
            last_pid = req->pid;  // Track the last PID for any necessary action
        }
    }

    // Clean up
    close(server_fifo_fd);
    unlink(server_fifo);
    fclose(log_file);
    closedir(dir);
    // Clean up resources
    return 0;
}

void init_shared_data() {
    // Map the shared memory object
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

    // Initialize the file semaphore
    if (sem_init(&shared_data->file_sem, 1, 1) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    // Initialize the queue semaphore
    if (sem_init(&shared_data->queue_sem, 1, 1) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
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

void handle_help(const char* command, struct response* resp) {
    if(strcmp(command, "list") == 0){
        strcpy(resp->data, "list: List the files in the server directory");
    } else if(strcmp(command, "readF") == 0){
        strcpy(resp->data, "readF <file> <line #>:\n display the #th line of the file");
    } else if(strcmp(command, "writeT") == 0){
        strcpy(resp->data, "writeT <file> <line #> <string>: request to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file.");
    } else if(strcmp(command, "upload") == 0){
        strcpy(resp->data, "upload <file>: Upload the file to the server");
    } else if(strcmp(command, "download") == 0){
        strcpy(resp->data, "download <file>: Download the file from the server");
    } else if(strcmp(command, "archServer") == 0){
        strcpy(resp->data, "archServer: Archive the server files");
    } else if(strcmp(command, "killServer") == 0){
        strcpy(resp->data, "killServer: Kill the server");
    } else if(strcmp(command, "quit") == 0){
        strcpy(resp->data, "quit: Send write request to Server side log file and quits");
    } else {
        strcpy(resp->data, "Invalid command");
        resp->status = RESP_ERROR;
    }
}

void handle_readF(char* cmd_args[], struct response* resp, int num_args) {
    if (num_args == 0) {
        strcpy(resp->data, "Invalid number of arguments");
        resp->status = RESP_ERROR;
        return;
    } else if (num_args == 1) {
        char* filename = cmd_args[0];
        strcpy(resp->data, readF(filename, dir));
    } else {
        char* filename = cmd_args[0];
        int line_num = atoi(cmd_args[1]);
        strcpy(resp->data, readF_Line(filename, line_num));
    }

    printf("resp->data: %s\n", resp->data);
}

char* readF(const char *filename, DIR *server_dir) {
    printf("readF %s\n", filename);

    char full_path[PATH_MAX];
    sprintf(full_path, "%s/%s", server_dir, filename);

    sem_wait(&file_sem); // Wait for the semaphore

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        sem_post(&file_sem); // Release the semaphore
        char* error_msg = strdup("Could not open file\n");
        return error_msg;
    }

    char line[MAX_LINE_LEN];
    ssize_t n;
    char c;
    char* file_contents = malloc(MAX_LINE_LEN);
    int i = 0;
    while ((n = read(fd, &c, 1)) > 0 && i < MAX_LINE_LEN - 1) {
        file_contents[i] = c;
        i++;
    }
    file_contents[i] = '\0';
    close(fd);

    sem_post(&file_sem); // Release the semaphore

    return file_contents;
}


char* readF_Line(const char *filename, int line_num) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        char* error_msg = strdup("Could not open file\n");
        return error_msg;
    }

    char line[MAX_LINE_LEN];
    int current_line = 0;
    ssize_t n;
    char c;
    // Read specific line
    int i = 0;
    while ((n = read(fd, &c, 1)) > 0) {
        if (c == '\n') {
            current_line++;
            if (current_line == line_num) {
                line[i] = '\0';
                close(fd);
                return strdup(line);
            }
            i = 0;  // Reset the index for the next line
        } else if (i < MAX_LINE_LEN - 1) {
            line[i] = c;
            i++;
        }
    }

    close(fd);
    return NULL;
}

struct response handle_command(const char* command, DIR* dir) {
    struct response resp = {RESP_OK, ""};

    Command parsed_cmd;
    parse_command(command, &parsed_cmd);

    if (strcmp(parsed_cmd.cmd, "help") == 0) {
        if(parsed_cmd.arg_count == 1){
            handle_help(parsed_cmd.args[0], &resp);
        } else if (parsed_cmd.arg_count > 1){
            strcpy(resp.data, "Invalid number of arguments");
            resp.status = RESP_ERROR;
        } else {
            strcpy(resp.data, "help, list, readF, writeT, upload, download, archServer, quit, killServer");
        }
    } else if (strcmp(command, "list") == 0) {
        return cmd_list(client_fd, dir);
    } else if (strncmp(command, "readF", 5) == 0) {
        printf("readF command\n");
        handle_readF(parsed_cmd.args, &resp, parsed_cmd.arg_count);
        printf("resp->data: %s\n", resp.data);
    } else if (strncmp(command, "writeT", 6) == 0) {
        strcpy(resp.data, "Writing to the #th line the file...");
    } else if (strncmp(command, "upload", 6) == 0) {
        strcpy(resp.data, "Uploading the file...");
    } else if (strncmp(command, "download", 8) == 0) {
        strcpy(resp.data, "Downloading the file...");
    } else if (strncmp(command, "archServer", 10) == 0) {
        strcpy(resp.data, "Archiving the server files...");
    } else if (strcmp(command, "killServer") == 0) {
        strcpy(resp.data, "Killing the server...");
    } else if (strcmp(command, "quit") == 0) {
        strcpy(resp.data, "Quitting...");
        resp.status = RESP_QUIT;
    } else {
        strcpy(resp.data, "Invalid command");
    }
    return resp;
}

struct response cmd_list(int client_fd, DIR *server_dir) {
    struct response resp;
    struct dirent *entry;
    char buf[BUF_SIZE];

    memset(resp.data, 0, sizeof(resp.data));  // Initialize the buffer
    resp.status = RESP_OK;

    rewinddir(server_dir);  // Reset the directory stream to the beginning
    printf(">> Listing files in the server directory\n");

    while ((entry = readdir(server_dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Only list regular files
            if (strlen(resp.data) + strlen(entry->d_name) + 1 < sizeof(resp.data)) {
                strcat(resp.data, entry->d_name);
                strcat(resp.data, "\n");
            } else {
                // Handle buffer overflow case, maybe set an error or warning
                strcpy(resp.data, "Error: Buffer overflow occurred");
                resp.status = RESP_ERROR;
                break;
            }
        }
    }

    // Send the list to the client
    if (write(client_fd, &resp, sizeof(resp)) < 0) {
        perror("write");
        resp.status = RESP_ERROR;
        strcpy(resp.data, "Failed to send list");
    }

    return resp;
}

void parse_command(char* request, Command* cmd) {
    char* token;
    int arg_count = 0;

    // Initialize the Command struct
    memset(cmd, 0, sizeof(Command));

    // Tokenize the command
    token = strtok(request, " ");
    if (token != NULL) {
        strncpy(cmd->cmd, token, CMD_LEN - 1);
        cmd->cmd[CMD_LEN - 1] = '\0';  // Ensure null termination
    }

    // Tokenize the arguments
    while ((token = strtok(NULL, " ")) != NULL) {
        strncpy(cmd->args[arg_count], token, BUF_SIZE - 1);
        cmd->args[arg_count][BUF_SIZE - 1] = '\0';  // Ensure null termination
        arg_count++;
        if (arg_count >= CMD_ARG_MAX) {
            break;
        }
    }

    cmd->arg_count = arg_count;

    printf("Command: %s\n", cmd->cmd);
    for (int i = 0; i < arg_count; i++) {
        printf("Arg %d: %s\n", i, cmd->args[i]);
    }

    // The command ID and file name will need to be set based on your specific requirements
    // For example:
    // cmd->cmd_id = ...;
    // strncpy(cmd->file_name, ..., BUF_SIZE - 1);
    // cmd->file_name[BUF_SIZE - 1] = '\0';  // Ensure null termination
}

void handle_client_request(struct request* req,  int server_fifo_fd, DIR* dir) {
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
        resp = handle_command(req->data, dir);

        if (write(client_fd, &resp, sizeof(resp)) != sizeof(resp)) {
            perror("write");
        }

        close(client_fd);  // Close the FIFO

        // Check if client sent a "Quitting..." command
        if (strcmp(resp.data, "Quitting...") == 0) {
            char message[BUF_SIZE];
            snprintf(message, BUF_SIZE, "Child %d - Client%d disconnected", req->pid, shared_data->child_count);
            write_log(message);
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


    // Destroy semaphore
    sem_destroy(&shared_data->sem);
    sem_destroy(&shared_data->file_sem);
    sem_destroy(&shared_data->queue_sem);

    // Unmap and unlink shared memory
    munmap(shared_data, sizeof(SharedData));
    close(shm_fd);
    shm_unlink(SHM_NAME);


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


