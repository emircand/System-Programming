#include "hw01.h"
#include "addStudent.h"
#include "sortList.h"
#include "searchStudent.h"

void createGradesFile(const char *fileName) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd == -1) {
            perror("Error creating file");
            exit(EXIT_FAILURE);
        }
        close(fd);
        exit(EXIT_SUCCESS); // Terminate child process upon successful file creation
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            printf("File '%s' created successfully.\n", fileName);
        } else {
            printf("Failed to create file '%s'.\n", fileName);
        }
    }
}

void logging(const char *message) {
    int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    if (log_fd == -1) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    if (write(log_fd, message, strlen(message)) == -1) {
        perror("Error writing to log file");
        exit(EXIT_FAILURE);
    }

    // Close the log file
    if (log_fd != -1) {
        close(log_fd);
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
                               " CHECK >  listSome \"filename\" \"numOfEntries\" \"pageNumber\" \n";

    write(STDOUT_FILENO, usageMessage, strlen(usageMessage));
}

int main() {
    char input[MAX_COMMAND_LENGTH];
    char command[MAX_COMMAND_LENGTH];

    while (1) {
        // Read user input
        write(STDOUT_FILENO, "> ", 2);
        ssize_t bytes_read = read(STDIN_FILENO, input, sizeof(input));
        if (bytes_read == -1) {
            perror("read");
            return 1;
        }
        input[bytes_read - 1] = '\0'; // Removing newline character

        if (strncmp(input, "addStudentGrade ", strlen("addStudentGrade ")) == 0) {
            handleAddStudentGrade(input);
        }  else if (strncmp(input, "searchStudent ", strlen("searchStudent ")) == 0) {
            handleSearchStudent(input);
        }  else if (strncmp(input, "sortAll ", strlen("sortAll ")) == 0) {
            handleSortAll(input);
        } else if(strncmp(input, "showAll ", strlen("showAll ")) == 0){
            handleShowAll(input);
        } else if (strncmp(input, "listGrades ", strlen("listGrades ")) == 0) {
            handleListGrades(input);
        } else if (strncmp(input, "listSome ", strlen("listSome ")) == 0) {
            handleListSome(input);
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
                logging("File created successfully.\n");
            } else {
                const char *unknownCommandMsg = "Unknown command. Please try again.\n";
                const char *usageMsg = "Usage: gtuStudentGrades <filename>\n";

                write(STDOUT_FILENO, unknownCommandMsg, strlen(unknownCommandMsg));
                write(STDOUT_FILENO, usageMsg, strlen(usageMsg));
            }
        }
    }
    return 0;
}