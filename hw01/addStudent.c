#include "addStudent.h"
#include "hw01.h"

void handleAddStudentGrade(char* input) {
    // Parsing input manually
    char *ptr = input + strlen("addStudentGrade ") + 1; // Skip the command
    char *filename = strtok(ptr, "\"");
    ptr = strtok(NULL, "\"");
    char *name = strtok(NULL, "\"");
    ptr = strtok(NULL, "\"");
    char *grade = strtok(NULL, "\"");

    if (filename != NULL && name != NULL && grade != NULL) {
        addInChildProcess(addStudentGrade, filename, name, grade);
    } else {
        write(STDOUT_FILENO, "Invalid command. Please use 'addStudentGrade \"filename\" \"Name Surname\" \"AA\"'.\n", strlen("Invalid command. Please use 'addStudentGrade \"filename\" \"Name Surname\" \"AA\"'.\n"));
    }
}

void addInChildProcess(void (*func)(const char *, const char *, const char *), const char *filename, const char *studentName, const char *grade) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        logging("addInChildProcess", "Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        func(filename, studentName, grade);
        exit(EXIT_SUCCESS); // Terminate child process upon successful execution
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            logging("addInChildProcess", "Error waiting for child process");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            write(STDOUT_FILENO, "Operation on '", strlen("Operation on '"));
            write(STDOUT_FILENO, filename, strlen(filename));
            write(STDOUT_FILENO, "' completed successfully.\n", strlen("' completed successfully.\n"));
        } else {
            write(STDOUT_FILENO, "Operation on '", strlen("Operation on '"));
            write(STDOUT_FILENO, filename, strlen(filename));
            write(STDOUT_FILENO, "' failed.\n", strlen("' failed.\n"));
        }
    }
}

void addStudentGrade(const char *filename, const char *studentName, const char *grade) {
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening file");
        logging("addStudentGrade", "Error opening file");
        exit(EXIT_FAILURE);
    }

    char buffer[100]; // Assuming a maximum of 100 characters for name and grade
    int len = snprintf(buffer, sizeof(buffer), "\"%s\" \"%s\"\n", studentName, grade);
    if (write(fd, buffer, len) != len) {
        perror("Error writing to file");
        logging("addStudentGrade", "Error writing to file");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

