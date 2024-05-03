#include "neHosLib.h"
#include "queue.h"

FILE* log_file = NULL;
struct Queue* child_pids;

int max_clients;
int server_fifo_fd, client_fifo_fd, num_read, num_write;
int server_fd, dummy_fd, client_fd, log_fd, dir_fd;
DIR* dir;
char dir_path[BUF_SIZE], server_fifo[BUF_SIZE];

sem_t file_sem, sem;
int shm_fd;
SharedData* shared_data;
int cleanup_flag = 0;
int client_number;


int main(int argc, char* argv[]) {
    
    char buf[BUF_SIZE];
    
    setup_signals();

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dirname> <max. #ofClients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* dir_name = argv[1];
    max_clients = atoi(argv[2]);
    child_pids = createQueue(max_clients);

    init_shared_data();
    
    /* Create the server folder if it doesn't exist */
    if ((mkdir(dir_name, S_IRWXU | S_IWUSR | S_IRUSR | S_IXUSR | S_IWGRP | S_IRGRP)) == -1 && errno != EEXIST)
        err_exit("mkdir");

    /* Open server folder */
    if ((dir = opendir(dir_name)) == NULL)
        err_exit("opendir");

    // Create a log file
    char log_path[BUF_SIZE];
    strcpy(dir_path, argv[1]);
    strcpy(log_path, dir_path);
    strcat(log_path, "/log.txt");

    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Print the server's PID
    printf(">> Server Started PID: %d\n", getpid());
    printf(">> Waiting for clients...\n");

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

    // Initialize child_count
    shared_data->child_count = 0;
    client_number = 0;

    while (!cleanup_flag) {
        struct request req;

        // Read the request from the server FIFO
        ssize_t numRead = read(server_fifo_fd, &req, sizeof(struct request));
        if (numRead <= 0) {
            if (numRead == 0) {
                fprintf(stderr, "Unexpected EOF\n");
                sleep(1);
                continue;
            } else {
                perror("read");
            }
            break;
        }

        // Enqueue the incoming request
        enqueue(child_pids, &req);
        
        // Dequeue a request
        req = *dequeue(child_pids);

        // Handle connection request
        if (req.type == CONNECT || req.type == TRY_CONNECT) {
            // Server side: Handling a connection request
            sem_wait(&shared_data->sem);
            if (shared_data->child_count < max_clients) {
                shared_data->child_count++;
                req.client_number = ++client_number;
                sem_post(&shared_data->sem);
                printf(">> Child PID: %d connected as client%d\n", req.pid, req.client_number);
                char message[BUF_SIZE];
                snprintf(message, BUF_SIZE, "Child %d - Client%d connected", req.pid, req.client_number);
                write_log(message);
                send_connect_response(req.pid, false);  // Send success response
            } else {
                sem_post(&shared_data->sem);
                printf(">> Connection request PID %d… Queue FULL \n", req.pid);
                send_connect_response(req.pid, true);  // Send failure response
            }
        }

        // Handle command request
        if (req.type == COMMAND) {
            handle_client_request(&req, server_fifo_fd, dir);
        }

        if(req.cmd == KILL_SERVER){
            printf(">>Server killed\n");
            break;
        }

        // sem_post(&shared_data->queue_sem);
    }
    printf(">> bye\n");
    cleanup();
    return 0;
}

void init_shared_data() {
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

void handle_readF(Command cmd, struct response* resp) {
    if (cmd.arg_count == 0) {
        strcpy(resp->data, "Invalid number of arguments");
        resp->status = RESP_ERROR;
        return;
    } else if (cmd.arg_count == 1) {
        char filename[MAX_FILES];
        strcpy(filename, cmd.args[0]);
        char file_contents[MAX_LINE_LEN];
        strcpy(resp->data, readF(filename, NULL));
    } else {
        char filename[MAX_FILES];
        strcpy(filename, cmd.args[0]);
        int line_num = atoi(cmd.args[1]);
        strcpy(resp->data, readF(filename, &line_num));
    }
}

char* readF(const char *filename, int *line_num) {
    char full_path[PATH_MAX];
    sprintf(full_path, "%s/%s", dir_path, filename);
    sem_wait(&shared_data->file_sem); // Wait for the semaphore
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        sem_post(&shared_data->file_sem); // Release the semaphore
        char* error_msg = strdup("Could not open file\n");
        return error_msg;
    }

    char* line = malloc(BUF_SIZE + 1);  // Allocate memory for one buffer
    if (line == NULL) {  // Check if malloc was successful
        close(fd);
        sem_post(&shared_data->file_sem); // Release the semaphore
        return NULL;
    }

    int current_line = 0;
    ssize_t n;
    int i = 0;

    if (line_num != NULL) {
        char* line_start = line;
        while ((n = read(fd, line_start, BUF_SIZE)) > 0) {
            for (i = 0; i < n; i++) {
                if (line_start[i] == '\n') {
                    current_line++;
                    if (current_line == *line_num) {
                        line_start[i] = '\0';  // Null-terminate the line
                        memmove(line, line_start, i+1);  // Move the line to the start of the buffer
                        break;
                    } else {
                        line_start = &line_start[i+1];  // Move the start of the line to the next character
                    }
                }
            }
            if (current_line == *line_num) {
                break;
            }
        }
        if (current_line != *line_num) {
            free(line);
            line = NULL;
        }
    } else {
        // Read the whole file
        while ((n = read(fd, line, BUF_SIZE)) > 0) {
            line[n] = '\0';  // Null-terminate the buffer
        }
    }
    close(fd);
    sem_post(&shared_data->file_sem); // Release the semaphore
    return line;
}

void handle_writeT(Command cmd, struct response* resp) {
    char filename[MAX_FILES];
    strcpy(filename, cmd.args[0]);
    int line_num = atoi(cmd.args[1]);

    if(cmd.arg_count == 2){
        char str[MAX_LINE_LEN];
        strcpy(str, cmd.args[1]);
        if (writeT(filename, line_num, str) == -1) {
            strcpy(resp->data, "Error writing to file");
            resp->status = RESP_ERROR;
        } else {
            char message[BUF_SIZE];
            sprintf(message, "Writing to the end of the file...");
            strcpy(resp->data, message);
        }
    } else if (cmd.arg_count == 3) {
        char str[MAX_LINE_LEN];
        strcpy(str, cmd.args[2]);
        if (writeT(filename, line_num, str) == -1) {
            strcpy(resp->data, "Error writing to file");
            resp->status = RESP_ERROR;
        } else {
            char message[BUF_SIZE];
            sprintf(message, "Writing to the %dth line of the file...", line_num);
            strcpy(resp->data, message);
        }
    } else {
        strcpy(resp->data, "Invalid number of arguments");
        resp->status = RESP_ERROR;
    }
}

int writeT(const char *filename, int line_num, const char *str) {
    char full_path[PATH_MAX];
    sprintf(full_path, "%s/%s", dir_path, filename);
    sem_wait(&shared_data->file_sem); // Wait for the semaphore
    int fd = open(full_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;
    }
    char c;
    ssize_t n;
    int current_line = 1;
    int i = 0;
    if (line_num == 0){
        lseek(fd, 0, SEEK_END);  // Move the file pointer to the end of the file
    } else if (line_num > 0) {
        while ((n = read(fd, &c, 1)) > 0) {
            if (c == '\n') {
                current_line++;
                if (current_line == line_num) {
                    break;
                }
                i = 0;  // Reset the index for the new str
            }
        }
        if (current_line != line_num) {
            close(fd);
            sem_post(&shared_data->file_sem); // Release the semaphore
            return -1;
        }
        lseek(fd, -1, SEEK_CUR);  // Move the file pointer back one byte
    } else {
        close(fd);
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;
    }

    char new_str[strlen(str) + 2];  // Create a new string that can hold the original string and a newline character
    new_str[0] = '\n';  // Add a newline character to the beginning of the new string
    strcpy(new_str + 1, str);  // Copy the original string to the new string, starting from the second character

    write(fd, new_str, strlen(new_str));  // Write the new string
    close(fd);
    sem_post(&shared_data->file_sem); // Release the semaphore
    return 0;
}

void handle_upload(Command cmd, struct response* resp) {
    if (cmd.arg_count != 1) {
        strcpy(resp->data, "Invalid number of arguments");
        resp->status = RESP_ERROR;
        return;
    }

    printf(">> File upload request received\n");
    printf(">> Uploading file: %s\n", cmd.args[0]);
    ssize_t bytes_transferred = uploadFile(cmd.args[0]);
    if (bytes_transferred == -1) {
        strcpy(resp->data, "Error uploading file");
        resp->status = RESP_ERROR;
    } else {
        char message[BUF_SIZE];
        sprintf(message, "%zd bytes uploaded", bytes_transferred);
        printf(">> %s\n", message);
        strcpy(resp->data, message);
    }
}

ssize_t uploadFile(const char *source_filename) {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    strcpy(source_path, source_filename);
    sprintf(dest_path, "%s/%s", dir_path, source_filename);

    sem_wait(&shared_data->file_sem); // Wait for the semaphore

    // Check if the source file exists
    if (access(source_path, F_OK) == -1) {
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;  // Return an error code if the source file does not exist
    }

    // Check if a file with the same name exists on the server's side
    if (access(dest_path, F_OK) != -1) {
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;  // Return an error code if a file with the same name exists
    }

    int source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1) {
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;
    }

    int dest_fd = open(dest_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (dest_fd == -1) {
        close(source_fd);
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;
    }

    char buffer[CHUNK_SIZE];
    ssize_t n;
    ssize_t total = 0;

    while ((n = read(source_fd, buffer, CHUNK_SIZE)) > 0) {
        if (write(dest_fd, buffer, n) != n) {
            close(source_fd);
            close(dest_fd);
            sem_post(&shared_data->file_sem); // Release the semaphore
            return -1;
        }
        total += n;
    }

    close(source_fd);
    close(dest_fd);
    sem_post(&shared_data->file_sem); // Release the semaphore

    return total;
}

void handle_download(Command cmd, struct response* resp) {
    if (cmd.arg_count != 1) {
        strcpy(resp->data, "Invalid number of arguments");
        resp->status = RESP_ERROR;
        return;
    }

    if (downloadFile(cmd.args[0]) == -1) {
        strcpy(resp->data, "Error downloading file");
        resp->status = RESP_ERROR;
    } else {
        char message[BUF_SIZE];
        sprintf(message, "Downloading the file with a name %s...", cmd.args[0]);
        strcpy(resp->data, message);
    }
}

int downloadFile(const char *source_filename) {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    sprintf(source_path, "%s/%s", dir_path, source_filename);
    strcpy(dest_path, source_filename);

    sem_wait(&shared_data->file_sem); // Wait for the semaphore

    // Check if the source file exists
    if (access(source_path, F_OK) == -1) {
        printf(">> Error: Source file does not exist\n");
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;  // Return an error code if the source file does not exist
    }

    // Check if a file with the same name exists on the client's side
    if (access(dest_path, F_OK) != -1) {
        printf(">> Error: Destination file already exists\n");
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;  // Return an error code if a file with the same name exists
    }

    int source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1) {
        printf(">>Error: Could not open source file\n");
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;
    }

    int dest_fd = open(dest_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (dest_fd == -1) {
        printf(">>Error: Could not open destination file\n");
        close(source_fd);
        sem_post(&shared_data->file_sem); // Release the semaphore
        return -1;
    }

    char buffer[CHUNK_SIZE];
    ssize_t n;

    while ((n = read(source_fd, buffer, CHUNK_SIZE)) > 0) {
        if (write(dest_fd, buffer, n) != n) {
            printf(">>Error: Could not write to destination file\n");
            close(source_fd);
            close(dest_fd);
            sem_post(&shared_data->file_sem); // Release the semaphore
            return -1;
        }
    }

    close(source_fd);
    close(dest_fd);
    sem_post(&shared_data->file_sem); // Release the semaphore

    return 0;
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
        return cmd_list(dir);
    } else if (strncmp(command, "readF", 5) == 0) {
        handle_readF(parsed_cmd, &resp);
    } else if (strncmp(command, "writeT", 6) == 0) {
        handle_writeT(parsed_cmd, &resp);
    } else if (strncmp(command, "upload", 6) == 0) {
        handle_upload(parsed_cmd, &resp);
    } else if (strncmp(command, "download", 8) == 0) {
        handle_download(parsed_cmd, &resp);
    } else if (strncmp(command, "archServer", 10) == 0) {
        archiveServer(parsed_cmd.args[0]);
        strcpy(resp.data, "Archiving the server...");
    } else if (strcmp(command, "killServer") == 0) {
        strcpy(resp.data, "Kill");
        resp.status = RESP_KILL;
    } else if (strcmp(command, "quit") == 0) {
        strcpy(resp.data, "Quitting...");
        resp.status = RESP_QUIT;
    } else {
        strcpy(resp.data, "Invalid command");
    }
    return resp;
}

struct response cmd_list(DIR *server_dir) {
    struct response resp;
    struct dirent *entry;
    char buf[BUF_SIZE];

    memset(resp.data, 0, sizeof(resp.data));  // Initialize the buffer
    resp.status = RESP_OK;

    rewinddir(server_dir);  // Reset the directory stream to the beginning

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

    // Determine the command ID based on the command string
    if (strcmp(cmd->cmd, "help") == 0) {
        cmd->cmd_id = HELP;
    } else if (strcmp(cmd->cmd, "list") == 0) {
        cmd->cmd_id = LIST;
    } else if (strcmp(cmd->cmd, "readF") == 0) {
        cmd->cmd_id = READ_F;
    } else if (strcmp(cmd->cmd, "writeT") == 0) {
        cmd->cmd_id = WRITE_T;
    } else if (strcmp(cmd->cmd, "upload") == 0) {
        cmd->cmd_id = UPLOAD;
    } else if (strcmp(cmd->cmd, "download") == 0) {
        cmd->cmd_id = DOWNLOAD;
    } else if (strcmp(cmd->cmd, "quit") == 0) {
        cmd->cmd_id = QUIT;
    } else if (strcmp(cmd->cmd, "killServer") == 0) {
        cmd->cmd_id = KILL_SERVER;
    } else {
        cmd->cmd_id = -1;  // Invalid command
    }

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
        if (resp.status == RESP_QUIT || resp.status == RESP_KILL) {
            char message[BUF_SIZE];
            snprintf(message, BUF_SIZE, "Child %d - client%d disconnected", req->pid, req->client_number);
            printf(">> %s\n", message);
            write_log(message);
            shared_data->child_count--;  // Decrement child count
        } if(resp.status == RESP_KILL){
            killServer();
        }else if (resp.status == RESP_ERROR) {
            char message[BUF_SIZE];
            snprintf(message, BUF_SIZE, "Child %d - client%d sent an invalid command", req->pid, req->client_number);
            shared_data->child_count--;  // Decrement child count
            write_log(message);
        } else {
            char message[BUF_SIZE];
            snprintf(message, BUF_SIZE, "Child %d - client%d sent a valid command", req->pid, req->client_number);
            write_log(message);
        }
        sem_post(&shared_data->sem);  // Release semaphore
    } else if (pid > 0) {  // Parent process
        int status;
        waitpid(pid, &status, WNOHANG);  // Non-blocking wait
    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}

void archiveServer(char *fileName) {
    pid_t pid = fork();
    char archive_path[PATH_MAX];
    sprintf(archive_path, "%s/%s", dir_path, fileName);

    if (pid < 0) {
        // Fork failed
        exit(1);
    } else if (pid == 0) {
        // Child process
        execlp("tar", "tar", "cf", archive_path, dir_path, NULL);
    } else {
        // Parent process
        wait(NULL);
    }
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = sig_handler; // Pointer to signal handler function
    sigfillset(&sa.sa_mask);    // Initialize the signal mask

    // Set up signal handlers for SIGINT, SIGTERM, SIGQUIT, and SIGCHLD:
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGCHLD, &sa, NULL) == -1 ){
        perror("Failed to set signal handlers");
        exit(EXIT_FAILURE);
        }
}

void cleanup() {
    destroyQueue(child_pids);
    // Close and unlink server FIFO
    if (server_fifo_fd >= 0) close(server_fifo_fd);
    if (dummy_fd >= 0) close(dummy_fd);

    closedir(dir);

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

    printf(">> Cleanup complete, server shutting down.\n");
    exit(EXIT_SUCCESS);
}

void sig_handler(int signum) {
    // Log the server shutdown
    write_log("Server Shutting Down");

    // Depending on the signal type, perform appropriate actions:
    switch (signum) {
        case SIGINT:
            printf(">> Handling sigint\n", signum);
            cleanup();
            break;
        case SIGCHLD:
            // Reap zombie processes and adjust child count:
            while(isEmpty(child_pids) == 0){
                struct request* req = dequeue(child_pids);
                if(waitpid(req->pid, NULL, WNOHANG) > 0){
                    char message[BUF_SIZE];
                    snprintf(message, BUF_SIZE, "Child %d - client%d disconnected", req->pid, req->client_number);
                    write_log(message);
                    kill(req->pid, SIGKILL);
                }
            }

            break;

        default:
            // Log unhandled signals:
            fprintf(stderr, "Unhandled signal: %d\n", signum);
            break;
    }
}

void err_exit(const char *err) 
{
    perror(err);
    exit(EXIT_FAILURE);
}

void killServer() {
    char message[BUF_SIZE];
    sprintf(message, "Server KILL", getpid());
    write_log(message);
    printf(">> Server killed\n");
    cleanup_flag = 1;
    if(kill(getppid(), SIGINT) == -1){
        perror("kill");
    }
}
