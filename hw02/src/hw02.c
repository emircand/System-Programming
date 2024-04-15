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
#include <time.h>
#include <signal.h>

#define FIFO1 "/tmp/fifo_ecd_1"
#define FIFO2 "/tmp/fifo_ecd_2"
#define BUF_LINE 50
#define MAX_SIZE 1000

static int counter = 0;
int sum = 0;
int product = 1;
char buffer[BUF_LINE];

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
        case SIGCHLD:
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                if (WIFEXITED(status)) {
                    printf("(Child PID: %d) exited with status %d\n", pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    printf("(Child PID: %d) killed by signal %d\n", pid, WTERMSIG(status));
                }
                counter++;
            }
            return;
        default:
            fprintf(stderr, "\nHandling SIGNUM: %d\n", signum);    
            break;
    }

    /* Send the SIGQUIT signal to all processes so that all the processes except shell process are terminated */
    kill(0, SIGQUIT);

    /* Wait for all child processes to terminate */
    while ((pid = waitpid(-1, NULL, 0)) != -1 || errno != ECHILD) {
        /* Print a message indicating that the child processes have been cleaned up */
        printf("(Child PID: %d) killed\n", pid);
    }

    /* Unlink the FIFOs */
    unlink(FIFO1);
    unlink(FIFO2);

    /* Exit the shell process */
    exit(EXIT_SUCCESS);
}


void write_to_fifo(const char *fifo, int *arr, int n) {
    int fd = open(fifo, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open FIFO for writing");
        exit(EXIT_FAILURE);
    }

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
    if (access(fifo_name, F_OK) != -1) {
        // FIFO exists, unlink it
        if (unlink(fifo_name) == -1) {
            perror("Failed to unlink existing FIFO");
            exit(EXIT_FAILURE);
        }
    }

    if (mkfifo(fifo_name, 0666) == -1) {
        perror(fifo_name);
        exit(EXIT_FAILURE);
    }
}

void child1(int n, char* buffer) {
    int fd1 = open(FIFO1, O_RDONLY | O_NONBLOCK);
    printf("Child 1 %d\n", getpid());
    if (fd1 < 0) {
        perror("Failed to open FIFO1 at read mode");
        exit(EXIT_FAILURE);
    }
    sleep(10);
    int sum_arr[n];
    for (int i = 0; i < n; i++) {
        if (read(fd1, &sum_arr[i], sizeof(int)) == -1) {
            perror("Failed to read from FIFO1");
            exit(EXIT_FAILURE);
        }
    }
    if (close(fd1) == -1) {
        perror("Failed to close FIFO1");
        exit(EXIT_FAILURE);
    }
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += sum_arr[i];
    }
    int length = sprintf(buffer, "Sum: %d\n", sum);
    write(STDOUT_FILENO, buffer, length);
    memset(buffer, 0, BUF_LINE);
    // Check if the FIFO exists
    if (access(FIFO2, F_OK) == -1) {
        perror("FIFO2 does not exist");
        exit(EXIT_FAILURE);
    }

    // Open the second FIFO and write the sum to it
    int fd2 = open(FIFO2, O_WRONLY | O_NONBLOCK);
    if (fd2 < 0) {
        perror("Failed to open FIFO2 on child 1");
        exit(EXIT_FAILURE);
    }
    if (write(fd2, &sum, sizeof(int)) == -1) {
        perror("Failed to write to FIFO2");
        exit(EXIT_FAILURE);
    }
    if (close(fd2) == -1) {
        perror("Failed to close FIFO2");
        exit(EXIT_FAILURE);
    } else {
        exit(EXIT_SUCCESS);
    }
}

void child2(char* cmd, char* buffer, int sum) {
    int fd2 = open(FIFO2, O_RDONLY | O_NONBLOCK);
    if (fd2 < 0) {
        perror("Failed to open FIFO2 at read mode");
        exit(EXIT_FAILURE);
    }
    printf("Child 2 %d\n", getpid());
    sleep(10);
    if (strcmp(cmd, "multiply") == 0) { 
        int arr[MAX_SIZE];
        int index = 0;
        while (read(fd2, &arr[index], sizeof(int)) > 0) {
            index++;
        }
        int product = 1;
        for (int i = 0; i < index; i++) {
            product *= arr[i];
            sum += arr[i];
        }
        int length = sprintf(buffer, "Product: %d\n", product);
        write(STDOUT_FILENO, buffer, length);
        memset(buffer, 0, BUF_LINE);
        // Print the sum of the results of all child processes
        length = sprintf(buffer, "Sum of results: %d\n", sum + product);
        write(STDOUT_FILENO, buffer, length);
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
}

int main(int argc, char *argv[]) {
    struct sigaction sa_ignore, sa_handle, sa_chld;
    /* ignore SIGQUIT in the shell process to kill child when it's necessary */
    sa_ignore.sa_flags = SA_SIGINFO | SA_RESTART;
    sa_ignore.sa_handler = SIG_IGN;
    if (sigemptyset(&sa_ignore.sa_mask) == -1 || 
        sigaction(SIGQUIT, &sa_ignore, NULL) == -1)
        perror("sa_ignore");

    /* handle SIGINT and SIGTERM */
    sa_handle.sa_flags = SA_SIGINFO | SA_RESTART;
    sa_handle.sa_handler = &sig_handler;
    if (sigemptyset(&sa_handle.sa_mask) == -1 || 
        sigaction(SIGINT, &sa_handle, NULL) == -1 || 
        sigaction(SIGTERM, &sa_handle, NULL) == -1)
        perror("sa_handle");
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n = atoi(argv[1]); // convert the argument to an integer
    int *arr = malloc(n * sizeof(int)); // allocate memory for the array

    srand(time(NULL)); // initialize the random number generator
    write(STDOUT_FILENO, "Array: ", 7);
    for (int i = 0; i < n; i++) {
        arr[i] = rand() % 8 + 1; // generate a random number between 1 and 8
        int length = sprintf(buffer, "%d ", arr[i]);
        write(STDOUT_FILENO, buffer, length);
        memset(buffer, 0, BUF_LINE);
    }
    int length = sprintf(buffer, "\n");
    write(STDOUT_FILENO, buffer, length);
    memset(buffer, 0, BUF_LINE);

    create_fifo(FIFO1);
    create_fifo(FIFO2);
    char cmd[10] = "multiply";

    /* handle SIGCHLD */
    sa_chld.sa_flags = SA_SIGINFO | SA_RESTART;
    sa_chld.sa_handler = &sig_handler;
    if (sigemptyset(&sa_chld.sa_mask) == -1 || 
        sigaction(SIGCHLD, &sa_chld, NULL) == -1)
        perror("sa_chld");
    
    /// Create two child processes
    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // read from FIFO and perform operation
            if(i==0){
                child1(n, buffer);
            }
            else if (i == 1) {
                child2(cmd, buffer, sum);
            }
            exit(EXIT_SUCCESS);
        } else {
            // Write to the FIFO immediately after creating the child process
            if(i==0){
                write_to_fifo(FIFO1, arr, n);
            }
            else if (i == 1) {
                write_to_fifo(FIFO2, arr, n);
            }
        }
    }

    // write_to_fifo(FIFO1, arr, n);
    // write_to_fifo(FIFO2, arr, n);
    free(arr);

    while (1) {
        printf("Proceeding\n");
        sleep(2);
        if (counter == 2) {
            int length = sprintf(buffer, "Program completed Successfully..\n");
            write(STDOUT_FILENO, buffer, length);
            memset(buffer, 0, BUF_LINE);
            exit(EXIT_SUCCESS);
            break;
        }
    }
    return 0;
}