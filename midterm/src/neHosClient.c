#include "neHosLib.h"
#include <stdbool.h>

sem_t sem;

void connect_to_server(int server_fd, bool wait);

int main(int argc, char *argv[]) {
    int client_fd, server_fd;
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
    server_fd = open(server_fifo, O_WRONLY);
    if (server_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    //connect_to_server(server_fd, strcmp(argv[1], "Connect") == 0);

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
        client_fd = open(client_fifo, O_RDONLY);
        if (client_fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // Read the server's response from the client FIFO
        char response[256];
        response[0] = '\0'; // Clear the response buffer
        if (read(client_fd, response, sizeof(response)) <= 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf(">>Response from server: %s\n", response);

        // Close the client FIFO
        close(client_fd);
        if (strcmp(response, "Quitting...") == 0) {
            printf(">>bye...\n");
            break;
        }
    }

    // Close the server FIFO
    close(server_fd);
    unlink(client_fifo);
    sem_destroy(&sem);

    return 0;
}

// Send a "Connect" or "tryConnect" request
void connect_to_server(int server_fd, bool wait) {
    sem_wait(&sem);

    write(server_fd, wait ? "Connect" : "tryConnect", wait ? 8 : 11);

    char response[256];
    read(server_fd, response, sizeof(response));

    if (strcmp(response, "Connected") == 0) {
        printf("Connected to the server\n");
    } else if (strcmp(response, "Full") == 0 && wait) {
        printf("Server is full, waiting for a spot to become available\n");
        read(server_fd, response, sizeof(response));
        printf("Connected to the server\n");
    } else {
        printf("Server is full, exiting\n");
        exit(EXIT_FAILURE);
    }

    sem_post(&sem);
}