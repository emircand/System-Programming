#include "neHosLib.h"

int main(int argc, char *argv[]) {
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
    int server_fd = open(server_fifo, O_WRONLY);
    if (server_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    //Wait for user input
    printf("Enter a command: ");
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
    printf("Request sent to server\n");

    // Close the server FIFO
    close(server_fd);

    // Open the client FIFO for reading
    int client_fd = open(client_fifo, O_RDONLY);
    if (client_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Read the server's response from the client FIFO
    char response[256];
    if (read(client_fd, response, sizeof(response)) <= 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    printf("Response from server: %s\n", response);

    // Close the client FIFO and delete it
    close(client_fd);
    unlink(client_fifo);

    return 0;
}