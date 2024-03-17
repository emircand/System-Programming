#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define MAX_NAME_LENGTH 50
#define MAX_GRADE_LENGTH 3
#define MAX_COMMAND_LENGTH 50
#define FILENAME "grades.txt"

void createGradesFile(const char *fileName) {
    int fd = open(fileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
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
    int len = snprintf(buffer, sizeof(buffer), "%s %s\n", studentName, grade);
    if (write(fd, buffer, len) != len) {
        perror("Error writing to file");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

void searchStudent(const char *studentName) {
    int file = open(FILENAME, O_RDONLY);
    if (file == -1) {
        printf("Error: Unable to open file %s\n", FILENAME);
        return;
    }

    char line[MAX_COMMAND_LENGTH];
    char name[MAX_NAME_LENGTH];
    int grade;
    int found = 0;
    ssize_t bytesRead;
    off_t offset = 0;

    struct stat fileStat;
    if (fstat(file, &fileStat) == -1) {
        perror("fstat");
        close(file);
        return;
    }

    while (offset < fileStat.st_size && (bytesRead = read(file, line, sizeof(line))) > 0) {
        off_t position = 0;
        while (position < bytesRead) {
            int tokenLength = strcspn(line + position, "\n");
            line[position + tokenLength] = '\0'; // Null-terminate the token
            sscanf(line + position, "%s %d", name, &grade);
            if (strcmp(name, studentName) == 0) {
                printf("Grade of %s: %d\n", studentName, grade);
                found = 1;
                break;
            }
            position += tokenLength + 1; // Move to the next token
        }
        offset += bytesRead;
        if (found) break;
        lseek(file, offset, SEEK_SET); // Move the file offset to the next read position
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

void logging(const char *logFileName, const char *message) {
    // Implement logging function here
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
        createGradesFile(fileName);
        printf("File %s created successfully.\n", fileName);
    }
    char input[MAX_COMMAND_LENGTH];
    while (1) {
        printf("\nEnter command: ");
        fgets(input, sizeof(input), stdin);

        // Remove trailing newline if present
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "addStudentGrade ", strlen("addStudentGrade ")) == 0) {
            char studentName[50], grade[3];
            if (sscanf(input, "addStudentGrade \"%MAX_NAME_LENGTH[^\"]\" \"%MAX_GRADE_LENGTH[^\"]\"", studentName, grade) == 2) {
                addStudentGrade(studentName, grade);
                printf("Student grade added successfully.\n");
            } else {
                printf("Invalid command format.\n");
            }
        } else {
            printf("Invalid command. Please use 'addStudentGrade \"Name Surname\" \"AA\"'.\n");
        }
    }

    return 0;
}
