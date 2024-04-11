#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <integer>\n", argv[0]);
        return 1;
    }
    
    int numChildren = atoi(argv[1]);
    int counter = 0;
    
    // Create two FIFOs
    mkfifo("fifo1", 0666);
    mkfifo("fifo2", 0666);
    
    // Open the first FIFO for writing
    int fifo1 = open("fifo1", O_WRONLY);
    
    // Open the second FIFO for writing
    int fifo2 = open("fifo2", O_WRONLY);
    
    // Send an array of random numbers to the first FIFO
    int randomNumbers[numChildren];
    for (int i = 0; i < numChildren; i++) {
        randomNumbers[i] = rand() % 100;
    }
    write(fifo1, randomNumbers, sizeof(randomNumbers));
    
    // Send a command to the second FIFO
    char command[] = "multiply";
    write(fifo2, command, sizeof(command));
    
    // Close the FIFOs
    close(fifo1);
    close(fifo2);
    
    // Fork child processes
    for (int i = 0; i < numChildren; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // Child process
            sleep(10);
            
            // Execute task
            
            exit(0);
        }
    }
    
    // Set signal handler for SIGCHLD
    signal(SIGCHLD, sigchldHandler);
    
    // Enter loop
    while (counter < numChildren) {
        printf("Proceeding...\n");
        sleep(2);
    }
    
    return 0;
}