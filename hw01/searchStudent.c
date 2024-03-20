#include "searchStudent.h"
#include "hw01.h"


void handleSearchStudent(char* input) {
    // Parsing input manually
    char *ptr = input + strlen("searchStudent ") + 1; // Skip the command
    char *filename = strtok(ptr, "\"");
    ptr = strtok(NULL, "\"");
    char *name = strtok(NULL, "\"");

    if (filename != NULL && name != NULL) {
        searchInChildProcess(searchStudent, filename, name);
    } else {
        write(STDOUT_FILENO, "Invalid command. Please use 'searchStudent \"filename\" \"Name Surname\"'.\n", strlen("Invalid command. Please use 'searchStudent \"filename\" \"Name Surname\"'.\n"));
    }
}

void searchInChildProcess(void (*func)(const char *, const char *), const char *filename, const char *studentName) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        func(filename, studentName);
        exit(EXIT_SUCCESS); // Terminate child process upon successful execution
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            write(STDOUT_FILENO, "Operation on '", strlen("Operation on '"));
            write(STDOUT_FILENO, filename, strlen(filename));
            write(STDOUT_FILENO, "' completed successfully.\n", strlen("' completed successfully.\n"));
        } else {
            printf("Operation on '%s' failed.\n", filename);
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