#include "hw01.h"
#include "addStudent.h"
#include "sortList.h"
#include "searchStudent.h"
#include <time.h>

void createGradesFile(const char *fileName) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        logging("createGradesFile", "Error creating file");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd == -1) {
            perror("Error creating file");
            logging("createGradesFile", "Error creating file");
            exit(EXIT_FAILURE);
        }
        close(fd);
        exit(EXIT_SUCCESS); // Terminate child process upon successful file creation
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            logging("createGradesFile", "Error waiting for child process");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            write(STDOUT_FILENO, "File created successfully.\n", 27);
        } else {
            write(STDOUT_FILENO, "Failed to create file.\n", 24);
        }
    }
}

void handleCommand(const char *command, void (*handler)(char *), char *input) {
    if (strncmp(input, command, strlen(command)) == 0) {
        handler(input);
        logging(command, "command executed successfully");
    }
}

void logging(const char *functionName, const char *message) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (log_fd == -1) {
            perror("Error opening log file");
            exit(EXIT_FAILURE);
        }

        char logMessage[256];
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        sprintf(logMessage, "[%02d:%02d:%02d] %s: %s\n", t->tm_hour, t->tm_min, t->tm_sec, functionName, message);

        if (write(log_fd, logMessage, strlen(logMessage)) == -1) {
            perror("Error writing to log file");
            exit(EXIT_FAILURE);
        }

        // Close the log file
        if (log_fd != -1) {
            close(log_fd);
        }

        exit(EXIT_SUCCESS); // Terminate child process upon successful logging
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            write(STDOUT_FILENO, "Logged successfully.\n", 21);
        } else {
            write(STDOUT_FILENO, "Failed to log.\n", 15);
        }
    }
}

void displayUsage() {
    const char *usageMessage = "Usage: <command> [<arguments>]\n"
                               "Available commands:\n"
                               ">  gtuStudentGrades \"filename\"\n"
                               ">  addStudentGrade \"studentName\" \"grade\"\n"
                               ">  searchStudent \"studentName\"\n"
                               ">  sortAll \"filename\" \"sortOption\" \"sortOrder\"\n"
                               ">  showAll \"filename\"\n"
                               ">  listGrades \"filename\"\n"
                               ">  listSome \"filename\" \"numOfEntries\" \"pageNumber\" \n";

    write(STDOUT_FILENO, usageMessage, strlen(usageMessage));
}

int main() {
    char input[MAX_COMMAND_LENGTH];
    char command[MAX_COMMAND_LENGTH];
    write(STDOUT_FILENO, "Welcome to GTU Student Grades Program\n", 38);
    

    while (1) {
        // Read user input
        write(STDOUT_FILENO, "To quit type 'exit'\n", 20);
        write(STDOUT_FILENO, "> ", 2);
        ssize_t bytes_read = read(STDIN_FILENO, input, sizeof(input));
        if (bytes_read == -1) {
            perror("read");
            return 1;
        }
        input[bytes_read - 1] = '\0'; // Removing newline character

        // Add exit command
        if (strncmp(input, "exit", 4) == 0) {
            logging("exit", "Exiting program");
            break;
        } else if (strncmp(input, "addStudentGrade ", strlen("addStudentGrade ")) == 0) {
            handleCommand("addStudentGrade", handleAddStudentGrade, input);
        }  else if (strncmp(input, "searchStudent ", strlen("searchStudent ")) == 0) {
            handleCommand("searchStudent", handleSearchStudent, input);
        }  else if (strncmp(input, "sortAll ", strlen("sortAll ")) == 0) {
            handleCommand("sortAll", handleSortAll, input);
        } else if(strncmp(input, "showAll ", strlen("showAll ")) == 0){
            handleCommand("showAll", handleShowAll, input);
        } else if (strncmp(input, "listGrades ", strlen("listGrades ")) == 0) {
            handleCommand("listGrades", handleListGrades, input);
        } else if (strncmp(input, "listSome ", strlen("listSome ")) == 0) {
            handleCommand("listSome", handleListSome, input);
        } else {
            // Parse input string to extract command
            char *token = strtok(input, " ");
            if (token == NULL) {
                const char *invalidCommandMsg = "Invalid command. Please try again.\n";
                write(STDOUT_FILENO, invalidCommandMsg, strlen(invalidCommandMsg));
                continue;
            }
            strcpy(command, token);
            if (strcmp(command, "gtuStudentGrades") == 0) {
                char *fileName = strtok(NULL, " ");
                if (fileName == NULL) {
                    displayUsage();
                    continue;
                }
                // Remove quotes from the filename
                char cleanedFileName[50];
                int i = 0, j = 0;
                while (fileName[i] != '\0') {
                    if (fileName[i] != '"') {
                        cleanedFileName[j++] = fileName[i];
                    }
                    i++;
                }
                cleanedFileName[j] = '\0';

                char *args[] = {"gtuStudentGrades", cleanedFileName, NULL};
                createGradesFile(args[1]);
                logging("gtuStudentGrades", "File created successfully");
            } else {
                const char *unknownCommandMsg = "Unknown command. Please try again.\n";
                const char *usageMsg = "To Display Usage: gtuStudentGrades\n";

                write(STDOUT_FILENO, unknownCommandMsg, strlen(unknownCommandMsg));
                write(STDOUT_FILENO, usageMsg, strlen(usageMsg));
            }
        }
    }
    return 0;
}