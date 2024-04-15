#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"
#define MAX_BUF 256
#define BUF_LINE 50
#define MAX_SIZE 1000

int counter = 0;
char buffer[BUF_LINE];

void increment_counter() {
    counter++;
}

void sigchld_handler(int signo) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else {
            printf("Child %d terminated abnormally\n", pid);
        }
        increment_counter();
    }
}


void write_to_fifo(const char *fifo, int *arr, int n, const char *cmd) {
    int fd = open(fifo, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open FIFO for writing");
        exit(EXIT_FAILURE);
    }

    // if (cmd != NULL) {
    //     char cmd_with_markers[12]; // Assuming cmd is at most 10 characters long
    //     sprintf(cmd_with_markers, "#%s$", cmd);
    //     if (write(fd, cmd_with_markers, strlen(cmd_with_markers) + 1) == -1) {
    //         perror("Failed to write command to FIFO");
    //         exit(EXIT_FAILURE);
    //     }
    // }

    for (int i = 0; i < n; i++) {
        while (write(fd, &arr[i], sizeof(int)) == -1) {
            if (errno != EAGAIN) {
                perror("Failed to write to FIFO");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (close(fd) == -1) {
        perror("Failed to close FIFO");
        exit(EXIT_FAILURE);
    }
}

void create_fifo(const char* fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1) {
        perror(fifo_name);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n = atoi(argv[1]); // convert the argument to an integer
    int *arr = malloc(n * sizeof(int)); // allocate memory for the array

    srand(time(NULL)); // initialize the random number generator
    for (int i = 0; i < n; i++) {
        arr[i] = rand() % 8; // generate a random number between 0 and 99
        int length = sprintf(buffer, "%d ", arr[i]);
        write(STDOUT_FILENO, buffer, length);
        memset(buffer, 0, BUF_LINE);
    }
    int length = sprintf(buffer, "\n");
    write(STDOUT_FILENO, buffer, length);
    memset(buffer, 0, BUF_LINE);

    create_fifo(FIFO1);
    create_fifo(FIFO2);

    signal(SIGCHLD, sigchld_handler);

    int sum, product;
    
    // Create two child processes
    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // read from FIFO and perform operation
            if(i==0){
                int fd1 = open(FIFO1, O_RDONLY);
                printf("sum %d\n", getpid());
                sleep(10);
                if (fd1 < 0) {
                    perror("Failed to open FIFO1");
                    exit(EXIT_FAILURE);
                }
                int arr[n];
                for (int i = 0; i < n; i++) {
                    if (read(fd1, &arr[i], sizeof(int)) == -1) {
                        perror("Failed to read from FIFO1");
                        exit(EXIT_FAILURE);
                    }
                }
                if (close(fd1) == -1) {
                    perror("Failed to close FIFO1");
                    exit(EXIT_FAILURE);
                }
                sum = 0;
                for (int i = 0; i < n; i++) {
                    sum += arr[i];
                }
                int length = sprintf(buffer, "Sum: %d\n", sum);
                write(STDOUT_FILENO, buffer, length);
                memset(buffer, 0, BUF_LINE);

                // Open the second FIFO and write the sum to it
                int fd2 = open(FIFO2, O_WRONLY);
                if (fd2 < 0) {
                    perror("Failed to open FIFO2");
                    exit(EXIT_FAILURE);
                }
                if (write(fd2, &sum, sizeof(int)) == -1) {
                    perror("Failed to write to FIFO2");
                    exit(EXIT_FAILURE);
                }
                if (close(fd2) == -1) {
                    perror("Failed to close FIFO2");
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_SUCCESS);
            }
            else if (i == 1) {
                int fd2 = open(FIFO2, O_RDONLY);
                printf("product %d\n", getpid());
                sleep(10);
                if (fd2 < 0) {
                    perror("Failed to open FIFO2");
                    exit(EXIT_FAILURE);
                }

                char cmd[15] = "#multiply"; // Increased size to accommodate special characters
                // char ch;
                // int cmd_index = 0;
                // while (read(fd2, &ch, sizeof(char)) > 0 && ch != '$') {
                //     cmd[cmd_index++] = ch;
                // }
                // cmd[cmd_index] = '\0'; // Null-terminate the string

                if (strcmp(cmd, "#multiply") == 0) { // Check for prepended '#'
                    int arr[MAX_SIZE];
                    int index = 0;
                    while (read(fd2, &arr[index], sizeof(int)) > 0) {
                        index++;
                    }
                    product = 1;
                    for (int i = 0; i < index; i++) {
                        product *= arr[i];
                    }
                    int length = sprintf(buffer, "Product: %d\n", product);
                    write(STDOUT_FILENO, buffer, length);
                    memset(buffer, 0, BUF_LINE);
                    if (close(fd2) == -1) {
                        perror("Failed to close FIFO2");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    perror("Invalid command");
                    if (close(fd2) == -1) {
                        perror("Failed to close FIFO2");
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_FAILURE);
                }
                // Print the sum of the results of all child processes
                int length = sprintf(buffer, "Sum of results: %d\n", sum + product);
                write(STDOUT_FILENO, buffer, length);
            }
            exit(EXIT_SUCCESS);
        }
    }

    write_to_fifo(FIFO1, arr, n, NULL);
    char cmd[] = "multiply";
    write_to_fifo(FIFO2, arr, n, cmd);

    while (1) {
        printf("Proceeding\n");
        sleep(2);
        if (counter == 2) {
            unlink(FIFO1);
            unlink(FIFO2);
            exit(EXIT_SUCCESS);
            break;
        }
    }
    return 0;
}