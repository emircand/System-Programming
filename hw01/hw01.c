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
#define FILENAME "grades.txt"
#define LOG_FILE "log.txt"


void createGradesFile(const char *fileName) {
    int fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        perror("Error creating file");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

void addStudentGrade(const char *studentName, const char *grade) {
    int fd = open(FILENAME, O_WRONLY | O_APPEND);
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
}

void searchStudent(const char *studentName) {
    int file = open(FILENAME, O_RDONLY);
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
            printf("Grade of %s: %s\n", studentName, grade);
            found = 1;
            break;
        }

        // Move the file pointer to the start of the next line
        off_t offset = lseek(file, 0, SEEK_CUR);
        lseek(file, offset - bytesRead + (closingQuoteGrade - buffer) + 1, SEEK_SET);
    }

    if (!found) {
        printf("Student '%s' not found.\n", studentName);
    }

    close(file);
}

void sortStudentGrades(const char *fileName, int sortOption, int sortOrder) {
    // Implement sortStudentGrades function here
}

void displayStudentGrades(const char *fileName) {
    // Implement displayStudentGrades function here
}

void logging(const char *message) {
    int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1) {
        perror("Error opening log file");
        return EXIT_FAILURE;
    }

    if (log_fd == -1) {
        perror("Error opening log file");
        return EXIT_FAILURE;
    }

    if (write(log_fd, message, strlen(message)) == -1) {
        perror("Error writing to log file");
        return EXIT_FAILURE;
    }

    // Close the log file
    if (log_fd != -1) {
        close(log_fd);
    }

}


void displayUsage() {
    // Implement displayUsage function here
}

int main(int argc, char *argv[]) {

    if (argc == 1) {
        displayUsage();
        return 0;
    }

    char *command = argv[1];
    char *fileName;
    if (argc > 2)
        fileName = argv[2];

    if (strcmp(command, "gtuStudentGrades") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s gtuStudentGrades <filename>\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            createGradesFile(fileName);
            logging("File created successfully.\n");
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
        }
    }

    char input[MAX_COMMAND_LENGTH];
    while (1) {
        printf("\nEnter command: ");
        fgets(input, sizeof(input), stdin);

        // Remove trailing newline if present
        input[strcspn(input, "\n")] = 0;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            if (strncmp(input, "addStudentGrade ", strlen("addStudentGrade ")) == 0) {
                char studentName[MAX_NAME_LENGTH];
                char grade[MAX_GRADE_LENGTH];
                if (sscanf(input, "addStudentGrade \"%49[^\"]\" \"%2[^\"]\"", studentName, grade) == 2) {
                    addStudentGrade(studentName, grade);
                    logging("Student grade added successfully.\n");
                } else {
                    logging("Invalid command. Please use 'addStudentGrade \"Name Surname\" \"AA\"'.\n");
                }
            } else if (strncmp(input, "searchStudent ", strlen("searchStudent ")) == 0) {
                char studentName[MAX_NAME_LENGTH];
                if (sscanf(input, "searchStudent \"%49[^\"]\"", studentName) == 1) {
                    searchStudent(studentName);
                    logging("Student Search done successfully.\n");
                } else {
                    logging("Invalid command. Please use 'searchStudent \"Name Surname\"\n");
                }
            }
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
        }
    }
    return 0;
}
