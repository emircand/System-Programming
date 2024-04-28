#include "neHosLib.h"
#include <stdbool.h>

sem_t sem;

void connect_to_server(int server_fd, bool wait);
void interact_with_server(int server_fd, const char* client_fifo, enum req_type type);

int main(int argc, char *argv[]) {
    int client_fd, server_fd;
    bool try_again = false;
    sem_init(&sem, 0, 1);
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> ServerPID\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    printf("Client PID: %d\n", getpid());
    struct request req;
    char client_fifo[256];
    char server_fifo[256];

    // Fill the request structure
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
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    printf("Client FIFO created: %s\n", client_fifo);

    // Create the server FIFO name
    snprintf(server_fifo, sizeof(server_fifo), SERVER_FIFO_TEMP, atoi(argv[2]));
    printf("Server FIFO: %s\n", server_fifo);

    // Open the server FIFO for writing
    server_fd = open(server_fifo, O_RDWR);
    if (server_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    connect_to_server(server_fd, try_again);

    // Close the server FIFO
    close(server_fd);
    unlink(client_fifo);
    sem_destroy(&sem);

    return 0;
}

void interact_with_server(int server_fd, const char* client_fifo, enum req_type type) {
    struct request req = { .pid = getpid(), .data = {0}, .type = type};

    while(1){
        //Wait for user input
        printf(">>Enter a command: ");
        if(fgets(req.data, sizeof(req.data), stdin) == NULL){
            perror("fgets");
            exit(EXIT_FAILURE);
        }

        // Remove the newline character
        req.data[strcspn(req.data, "\n")] = 0;

        // Write the request to the server FIFO
        if (write(server_fd, &req, sizeof(struct request)) != sizeof(struct request)) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        printf(">>Request sent to server\n");

        // Open the client FIFO for reading
        int client_fd = open(client_fifo, O_RDONLY);
        if (client_fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // Read the server's response from the client FIFO
        struct response resp;
        if (read(client_fd, &resp, sizeof(resp)) <= 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf(">>Response from server: %s\n", resp.data);

        // Close the client FIFO
        close(client_fd);
        if (strcmp(resp.data, "Quitting...") == 0) {
            printf(">>bye...\n");
            break;
        }
    }
}

// Send a "Connect" or "tryConnect" request
void connect_to_server(int server_fd, bool wait) {
    struct request req = { .pid = getpid(), .data = {0} };
    if (wait) {
        req.type = CONNECT;
    } else {
        req.type = TRY_CONNECT;
    }

    write(server_fd, &req, sizeof(req));

    // Open the client FIFO for reading
    char client_fifo[256];
    snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMP, req.pid);
    int client_fd = open(client_fifo, O_RDONLY);
    if (client_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
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
                printf("Connected to the server.\n");
                close(client_fd);
                interact_with_server(server_fd, client_fifo, COMMAND);
                return;
            case RESP_ERROR:
                if (strcmp(resp.data, "Server full") == 0) {
                    if (wait && req.type == CONNECT) {
                        printf(">>Server is full, waiting 5 seconds for a spot to become available...\n");
                        sleep(5); // wait for a spot to become available
                        connect_to_server(server_fd, true);
                    } else {
                        printf(">>Server is full, exiting.\n");
                        close(client_fd); // Close the client_fd before exiting
                        exit(EXIT_FAILURE);
                    }
                } else {
                    printf("Unexpected response from server: status=%d, data=%s\n", resp.status, resp.data);
                    close(client_fd); // Close the client_fd before exiting
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                printf("Unexpected response from server: status=%d, data=%s\n", resp.status, resp.data);
                close(client_fd); // Close the client_fd before exiting
                exit(EXIT_FAILURE);
        }
    }
}
