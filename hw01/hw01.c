#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/stat.h>

#define MAX_NAME_LENGTH 50
#define MAX_GRADE_LENGTH 3
#define MAX_COMMAND_LENGTH 50
#define MAX_LINE_LENGTH 1024
#define FILENAME "grades.txt"
#define LOG_FILE "log.txt"


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

void addStudentGrade(const char *filename, const char *studentName, const char *grade) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        char buffer[100]; // Assuming a maximum of 100 characters for name and grade
        int len = snprintf(buffer, sizeof(buffer), "\"%s\" \"%s\"\n", studentName, grade);
        if (write(fd, buffer, len) != len) {
            perror("Error writing to file");
            exit(EXIT_FAILURE);
        }
        close(fd);
        exit(EXIT_SUCCESS); // Terminate child process upon successful file write
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            printf("Student grade added to '%s' successfully.\n", filename);
        } else {
            printf("Failed to add student grade to '%s'.\n", filename);
        }
    }
}

void searchStudent(const char *fileName, const char *studentName) {
    int file = open(fileName, O_RDONLY);
    if (file == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int found = 0;
    while (1) {
        // Read a line from the file
        char buffer[256]; // Choose a sufficiently large buffer size
        ssize_t bytesRead = read(file, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            break; // End of file or error
        }

        // Null-terminate the buffer
        buffer[bytesRead] = '\0';

        // Find the opening quote of the name
        char *namePtr = strchr(buffer, '"');
        if (namePtr == NULL) {
            continue; // Invalid format, skip this line
        }
        namePtr++; // Move past the opening quote

        // Find the closing quote of the name
        char *closingQuoteName = strchr(namePtr, '"');
        if (closingQuoteName == NULL) {
            continue; // Invalid format, skip this line
        }
        // Calculate the length of the name
        size_t nameLength = closingQuoteName - namePtr;
        if (nameLength >= MAX_NAME_LENGTH) {
            continue; // Name too long, skip this line
        }

        char name[MAX_NAME_LENGTH];
        char grade[MAX_GRADE_LENGTH];

        // Copy the name into the 'name' array and add EOS
        memcpy(name, namePtr, nameLength);
        name[nameLength] = '\0';

        // Move to the space after the closing quote of the name
        char *spaceAfterName = closingQuoteName + 1;

        // Find the opening quote of the grade
        char *gradePtr = strchr(spaceAfterName, '"');
        if (gradePtr == NULL) {
            continue; // Invalid format, skip this line
        }
        gradePtr++; // Move past the opening quote

        // Find the closing quote of the grade
        char *closingQuoteGrade = strchr(gradePtr, '"');
        if (closingQuoteGrade == NULL) {
            continue; // Invalid format, skip this line
        }

        // Calculate the length of the grade
        size_t gradeLength = closingQuoteGrade - gradePtr;
        if (gradeLength >= MAX_GRADE_LENGTH) {
            continue; // Grade too long, skip this line
        }

        // Copy the grade into the 'grade' array and add EOS
        memcpy(grade, gradePtr, gradeLength);
        grade[gradeLength] = '\0';

        // Compare the extracted name with the studentName parameter
        size_t extractedNameLength = closingQuoteName - namePtr; // Adjust for quotes
        if (extractedNameLength != nameLength) {
            continue; // Name lengths don't match, skip this line
        }
        if (strncmp(namePtr, studentName, nameLength) == 0) {
            // Remove quotes from name
            name[extractedNameLength] = '\0'; // Adjust for the last quote
            char output[256];
            int len = snprintf(output, sizeof(output), "Grade of %s: %s\n", studentName, grade);
            write(STDOUT_FILENO, output, len);
            found = 1;
            break;
        }

        // Move the file pointer to the start of the next line
        off_t offset = lseek(file, 0, SEEK_CUR);
        lseek(file, offset - bytesRead + (closingQuoteGrade - buffer) + 1, SEEK_SET);
    }

    if (!found) {
        char output[256];
        int len = snprintf(output, sizeof(output), "Student '%s' not found.\n", studentName);
        write(STDOUT_FILENO, output, len);
    }

    close(file);
}

// Comparison function declarations
int compareByNameAsc(const void *a, const void *b);
int compareByNameDesc(const void *a, const void *b);
int compareByGradeAsc(const void *a, const void *b);
int compareByGradeDesc(const void *a, const void *b);

void sortStudentGrades(const char *filename, int sortBy, int sortOrder) {
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read lines into an array
    char lines[MAX_LINE_LENGTH][MAX_LINE_LENGTH];
    int numLines = 0;
    ssize_t bytesRead;
    char buf[MAX_LINE_LENGTH];

    while ((bytesRead = read(fd, buf, sizeof(buf))) > 0) {
        strcpy(lines[numLines], buf);
        numLines++;
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }

    // Sort lines based on sortBy and sortOrder
    if (sortBy == 0) { // Sort by student name
        qsort(lines, numLines, sizeof(lines[0]), sortOrder == 0 ? compareByNameAsc : compareByNameDesc);
    } else { // Sort by grade
        qsort(lines, numLines, sizeof(lines[0]), sortOrder == 0 ? compareByGradeAsc : compareByGradeDesc);
    }

    // Write sorted lines back to file
    lseek(fd, 0, SEEK_SET); // Move the file pointer to the beginning

    for (int i = 0; i < numLines; i++) {
        write(fd, lines[i], strlen(lines[i]));
    }

    close(fd);
}

// Comparison function definitions
int compareByNameAsc(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int compareByNameDesc(const void *a, const void *b) {
    return -strcmp((const char *)a, (const char *)b);
}

int compareByGradeAsc(const void *a, const void *b) {
    // Implement comparison logic for grades ascending order
    return -1;
}

int compareByGradeDesc(const void *a, const void *b) {
    // Implement comparison logic for grades descending order
    return -1;
}

void listGrades(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    ssize_t bytesRead;
    int count = 0;

    char header[100];
    int header_len = snprintf(header, sizeof(header), "First 5 entries from %s:\n", filename);
    write(STDOUT_FILENO, header, header_len);

    while ((bytesRead = read(fd, line, sizeof(line))) > 0 && count < 5) {
        write(STDOUT_FILENO, line, bytesRead);
        for (int i = 0; i < bytesRead; i++) {
            if (line[i] == '\n') {
                count++;
            }
            if (count == 5) {
                break;
            }
        }
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void listSome(const char *filename, int numOfEntries, int pageNumber) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    ssize_t bytesRead;
    int count = 0;
    int startEntry = (pageNumber - 1) * numOfEntries + 1;
    int endEntry = pageNumber * numOfEntries;

    char header[100];
    int header_len = snprintf(header, sizeof(header), "Entries from %d to %d from %s:\n", startEntry, endEntry, filename);
    write(STDOUT_FILENO, header, header_len);

    while ((bytesRead = read(fd, line, sizeof(line))) > 0) {
        for (int i = 0; i < bytesRead; i++) {
            if (line[i] == '\n') {
                count++;
            }
            if (count >= startEntry && count <= endEntry) {
                write(STDOUT_FILENO, &line[i], 1);
            }
            if (count > endEntry) {
                break;
            }
        }
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void showAll(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;
    int page = 0;
    ssize_t bytesRead;

    while ((bytesRead = read(fd, line, sizeof(line))) > 0) {
        write(STDOUT_FILENO, line, bytesRead);
        for (int i = 0; i < bytesRead; i++) {
            if (line[i] == '\n') {
                count++;
            }
            if (count == 5) {
                count = 0;
                page++;
                char pageHeader[50];
                int pageHeader_len = snprintf(pageHeader, sizeof(pageHeader), "\nPage %d:\n", page);
                write(STDOUT_FILENO, pageHeader, pageHeader_len);
            }
        }
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }

    close(fd);
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
                               ">  sortStudentGrades \"filename\" <sortOption> <sortOrder>\n"
                               ">  showAll \"filename\"\n"
                               ">  listGrades \"filename\"\n"
                               ">  listSome <numOfEntries> <pageNumber> \"filename\"\n";

    write(STDOUT_FILENO, usageMessage, strlen(usageMessage));
}

void handleFunctionCall_p2(void (*function)(char *, char *), char *arg1, char *arg2) {
    pid_t pid = fork(); // Create a new process
    if (pid == -1) {
        // Error handling for fork failure
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        function(arg1, arg2); // Call the provided function
        exit(EXIT_SUCCESS); // Terminate child process upon successful completion
    } else {
        // Parent process
        // Wait for the child process to complete
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            // Error handling for waitpid failure
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            // Child process terminated successfully
            printf("Function call succeeded.\n");
        } else {
            printf("Function call failed.\n");
        }
    }
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
            // Parsing input manually
            char *ptr = input + strlen("addStudentGrade ") + 1; // Skip the command
            char *filename = strtok(ptr, "\"");
            ptr = strtok(NULL, "\"");
            char *name = strtok(NULL, "\"");
            ptr = strtok(NULL, "\"");
            char *grade = strtok(NULL, "\"");

            if (filename != NULL && name != NULL && grade != NULL) {
                addStudentGrade(filename, name, grade);
            } else {
                write(STDOUT_FILENO, "Invalid command. Please use 'addStudentGrade \"filename\" \"Name Surname\" \"AA\"'.\n", strlen("Invalid command. Please use 'addStudentGrade \"filename\" \"Name Surname\" \"AA\"'.\n"));
            }
        }  else if (strncmp(input, "searchStudent ", strlen("searchStudent ")) == 0) {
            // Parsing input manually
            char *ptr = input + strlen("searchStudent ") + 1; // Skip the command
            char *filename = strtok(ptr, "\"");
            ptr = strtok(NULL, "\"");
            char *name = strtok(NULL, "\"");

            if (filename != NULL && name != NULL) {
                // Remove quotes from filename
                char cleanedFileName[50];
                int i = 0, j = 0;
                while (filename[i] != '\0') {
                    if (filename[i] != '"') {
                        cleanedFileName[j++] = filename[i];
                    }
                    i++;
                }
                cleanedFileName[j] = '\0';
                searchStudent(cleanedFileName, name);
                logging("Student search done successfully.\n");
            } else {
                write(STDOUT_FILENO, "Invalid command. Please use 'searchStudent \"filename\" \"Name Surname\"'\n", strlen("Invalid command. Please use 'searchStudent \"filename\" \"Name Surname\"'\n"));
            }
        } else if (strncmp(input, "sortStudentGrades ", strlen("sortStudentGrades ")) == 0) {
            // Parsing input manually
            char *ptr = input + strlen("sortStudentGrades ") + 1; // Skip the command
            char *name = strtok(ptr, "\"");
            ptr = strtok(NULL, "\"");
            char *sortOption = strtok(NULL, " ");
            ptr = strtok(NULL, " ");
            char *sortOrder = strtok(NULL, " ");

            if (name != NULL && sortOption != NULL && sortOrder != NULL) {
                char *args[] = {"sortStudentGrades", name, sortOption, sortOrder, NULL};
                sortStudentGrades(args[1], args[2], args[3]);
                logging("Student grades sorted successfully.\n");
            } else {
                write(STDOUT_FILENO, "Invalid command. Please use 'sortStudentGrades \"Name Surname\" <sortOption> <sortOrder>'\n", strlen("Invalid command. Please use 'sortStudentGrades \"Name Surname\" <sortOption> <sortOrder>'\n"));
            }
        } else if(strncmp(input, "showAll ", strlen("showAll ")) == 0){
            char *ptr = input + strlen("showAll ") + 1; // Skip the command
            char *filename = strtok(ptr, "\"");
            ptr = strtok(NULL, "\"");
            if (filename != NULL) {
                char *args[] = {"showAll", filename, NULL};
                showAll(args[1]);
                logging("showAll grades listed successfully.\n");
                } else {
                    write(STDOUT_FILENO, "Invalid command. Please use 'showAll \"filename\"'\n", strlen("Invalid command. Please use 'showAll \"filename\"'\n"));
                }
        } else if (strncmp(input, "listGrades ", strlen("listGrades ")) == 0) {
            char *ptr = input + strlen("listGrades ") + 1; // Skip the command
            char *filename = strtok(ptr, "\"");
            ptr = strtok(NULL, "\"");
            if (filename != NULL) {
                char *args[] = {"listGrades", filename, NULL};
                listGrades(args[1]);
                logging("listGrades grades listed successfully.\n");
            } else {
                write(STDOUT_FILENO, "Invalid command. Please use 'listGrades \"filename\"'\n", strlen("Invalid command. Please use 'listGrades \"filename\"'\n"));
            }
        } else if (strncmp(input, "listSome ", strlen("listSome ")) == 0) {
            int numOfEntries;
            int pageNumber;
            char *ptr = input + strlen("listSome ") + 1; // Skip the command
            char *arg1 = strtok(ptr, " ");
            char *arg2 = strtok(NULL, " ");
            char *filename = strtok(NULL, "\"");
            if (filename != NULL) {
                filename = strtok(NULL, "\"");
            }
            if (arg1 != NULL && arg2 != NULL) {
                numOfEntries = atoi(arg1);
                pageNumber = atoi(arg2);
                listSome(filename, numOfEntries, pageNumber);
                logging("listSome grades listed successfully.\n");
            } else {
                logging("Invalid command. Please use 'listSome <numOfEntries> <pageNumber> \"filename\"\n");
            }
        } else {
            // Parse input string to extract command
            char *token = strtok(input, " ");
            if (token == NULL) {
                const char *invalidCommandMsg = "Invalid command. Please try again.\n";
                write(STDOUT_FILENO, invalidCommandMsg, strlen(invalidCommandMsg));
                continue;
            }
            strcpy(command, token);
            // Your command processing logic here
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