#include "neHosLib.h"
#include <stdbool.h>

void connect_to_server(int server_fd, bool wait);
void interact_with_server(int server_fd, const char* client_fifo, enum req_type type);
sem_t file_sem, sem, queue_sem;
int shm_fd;
SharedData* shared_data;

char client_fifo[256];
char server_fifo[256];
int client_fd, server_fd;

int main(int argc, char *argv[]) {
    bool try_again = false;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> ServerPID\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    printf(">> Client PID: %d\n", getpid());

    // Set up signal handler
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        err_exit("sigaction");
    }

    struct request req;

    init_shared_data();

    req.pid = getpid();
    strcpy(req.data, argv[1]);
    if(strcmp(argv[1], "Connect") == 0){
        req.type = CONNECT;
        try_again = true;
    } else if(strcmp(argv[1], "tryConnect") == 0){
        req.type = TRY_CONNECT;
        try_again = false;
    } else {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> ServerPID\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    // Create the client FIFO
    snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMP, req.pid);
    if (mkfifo(client_fifo, 0666) == -1) {
        err_exit("mkfifo");
    }
    // printf(">> Client FIFO created: %s\n", client_fifo);

    // Create the server FIFO name
    snprintf(server_fifo, sizeof(server_fifo), SERVER_FIFO_TEMP, atoi(argv[2]));
    // printf(">> Server FIFO: %s\n", server_fifo);

    // Open the server FIFO for writing
    server_fd = open(server_fifo, O_WRONLY);
    if (server_fd == -1) {
        err_exit("open");
    }

    connect_to_server(server_fd, try_again);

    // Close the server FIFO
    close(server_fd);
    unlink(client_fifo);
    sem_destroy(&shared_data->sem);
    munmap(shared_data, sizeof(SharedData));

    return 0;
}

void err_exit(const char *err){
    perror(err);
    exit(EXIT_FAILURE);
}

void sig_handler(int signum){
    printf(">> Handling SIGINT\n");
    close(server_fd);
    sem_destroy(&shared_data->sem);
    munmap(shared_data, sizeof(SharedData));
    unlink(client_fifo);

    exit(EXIT_SUCCESS);
}

void init_shared_data() {
    // Open the shared memory object
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        err_exit("shm_open");
    }

    // Resize the shared memory object
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        err_exit("ftruncate");
    }

    // Map the shared memory object
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        err_exit("mmap");
    }

    // Initialize the semaphores
    sem_init(&shared_data->sem, 1, 1);
    sem_init(&shared_data->file_sem, 1, 1);
    sem_init(&shared_data->queue_sem, 1, 1);
}

void interact_with_server(int server_fd, const char* client_fifo, enum req_type type) {
    struct request req = { .pid = getpid(), .data = {0}, .type = type};

    while(1){
        //Wait for user input
        printf(">> Enter a command: ");
        if(fgets(req.data, sizeof(req.data), stdin) == NULL){
            err_exit("fgets");
        }

        // Remove the newline character
        req.data[strcspn(req.data, "\n")] = 0;

        // Write the request to the server FIFO
        if (write(server_fd, &req, sizeof(struct request)) != sizeof(struct request)) {
            err_exit("write");
        }

        // Open the client FIFO for reading
        int client_fd = open(client_fifo, O_RDONLY);
        if (client_fd == -1) {
            err_exit("open");
        }

        // Read the server's response from the client FIFO
        struct response resp = handle_response(client_fd);        

        // Close the client FIFO
        close(client_fd);
        if(resp.status == RESP_QUIT || resp.status == RESP_KILL){
            return;
        }
    }
}

struct response handle_response(int client_fd) {
    struct response resp;
    ssize_t n = read(client_fd, &resp, sizeof(resp));
    if (n < 0) {
        err_exit("read");
    } else if (n == 0) {
        printf(">> Server closed connection\n");
    }

    if (resp.status == RESP_ERROR) {
        printf(">> Error: %s\n", resp.data);
    } else {
        printf(">> Response: %s\n", resp.data);
    } 
    return resp;
}

// Send a "Connect" or "tryConnect" request
void connect_to_server(int server_fd, bool wait) {
    struct request req = { .pid = getpid(), .data = {0} };
    if (wait) {
        req.type = CONNECT;
    } else {
        req.type = TRY_CONNECT;
    }

    if (write(server_fd, &req, sizeof(struct request)) != sizeof(struct request)) {
        err_exit("write");
    }

    // Open the client FIFO for reading
    char client_fifo[256];
    snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMP, req.pid);
    int client_fd = open(client_fifo, O_RDONLY);
    if (client_fd == -1) {
        err_exit("open");
    }
    while (true) {
        struct response resp;
        ssize_t numRead = read(client_fd, &resp, sizeof(resp));
        if (numRead <= 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        switch (resp.status) {
            case RESP_CONNECT:
                printf(">> Connected to the server.\n");
                close(client_fd);
                interact_with_server(server_fd, client_fifo, COMMAND);
                return;
            case RESP_ERROR:
                if (strcmp(resp.data, "Server full") == 0) {
                    if (wait && req.type == CONNECT) {
                        printf(">> Server is full, waiting 5 seconds for a spot to become available...\n");
                        sleep(5); // wait for a spot to become available
                        connect_to_server(server_fd, true);
                    } else {
                        printf(">> Server is full, exiting.\n");
                        close(client_fd); // Close the client_fd before exiting
                        unlink(client_fifo); // Free the FIFO
                        sem_destroy(&shared_data->sem); // Destroy the semaphore
                        munmap(shared_data, sizeof(SharedData)); // Free the shared memory
                        exit(EXIT_FAILURE);
                    }
                } else {
                    printf(">> Unexpected response from server: status=%d, data=%s\n", resp.status, resp.data);
                    close(client_fd); // Close the client_fd before exiting
                    unlink(client_fifo); // Free the FIFO
                    sem_destroy(&shared_data->sem); // Destroy the semaphore
                    munmap(shared_data, sizeof(SharedData)); // Free the shared memory
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                printf(">> Unexpected response from server: status=%d, data=%s\n", resp.status, resp.data);
                close(client_fd); // Close the client_fd before exiting
                unlink(client_fifo); // Free the FIFO
                sem_destroy(&shared_data->sem); // Destroy the semaphore
                munmap(shared_data, sizeof(SharedData)); // Free the shared memory
                exit(EXIT_FAILURE);
        }
    }
}