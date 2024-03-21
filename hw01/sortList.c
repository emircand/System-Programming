#include "sortList.h"
#include "hw01.h"

typedef struct {
    char name[50];
    char grade[3];
} Student;

int sortBy;
int sortOrder;


void listGradesInChildProcess(void (*func)(const char *), const char *filename) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        logging("listGrades", "fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        func(filename);
        exit(EXIT_SUCCESS); // Terminate child process upon successful execution
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            logging("listGrades", "waitpid failed");
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

void listSomeInChildProcess(void (*func)(const char *, int, int), const char *filename, const char *numOfEntries, const char *pageNumber) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        logging("listSome", "fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int numOfEntriesInt = atoi(numOfEntries);
        int pageNumberInt = atoi(pageNumber);
        func(filename, numOfEntriesInt, pageNumberInt);
        exit(EXIT_SUCCESS); // Terminate child process upon successful execution
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            logging("listSome", "waitpid failed");
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

void showAllInChildProcess(void (*func)(const char *), const char *filename) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        logging("showAll", "fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        func(filename);
        exit(EXIT_SUCCESS); // Terminate child process upon successful execution
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            logging("showAll", "waitpid failed");
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

void handleSortAll(char* input) {
    char *ptr = input + strlen("sortAll ") + 1;
    char *filename = strtok(ptr, "\"");
    ptr = strtok(NULL, "\"");
    char *sortBy = strtok(NULL, "\"");
    ptr = strtok(NULL, "\"");
    char *sortOrder = strtok(NULL, "\"");

    if (filename != NULL && sortBy != NULL && sortOrder != NULL) {
        sortInChildProcess(sortAll, filename, sortBy, sortOrder);
    } else {
        write(STDOUT_FILENO, "Invalid command. Please use 'sortAll \"filename\" \"sortBy\" \"sortOrder\"'.\n", strlen("Invalid command. Please use 'sortAll \"filename\" \"sortBy\" \"sortOrder\"'.\n"));
    }
}

void handleListGrades(char* input) {
    char *ptr = input + strlen("listGrades ") + 1;
    char *filename = strtok(ptr, "\"");

    if (filename != NULL) {
        listGradesInChildProcess(listGrades, filename);
    } else {
        write(STDOUT_FILENO, "Invalid command. Please use 'listGrades \"filename\"'.\n", strlen("Invalid command. Please use 'listGrades \"filename\"'.\n"));
    }
}

void handleListSome(char* input) {
    char *ptr = input + strlen("listSome ") + 1;
    char *filename = strtok(ptr, "\"");
    ptr = strtok(NULL, "\"");
    char *numOfEntries = strtok(NULL, "\"");
    ptr = strtok(NULL, "\"");
    char *pageNumber = strtok(NULL, "\"");

    if (filename != NULL && numOfEntries != NULL && pageNumber != NULL) {
        listSomeInChildProcess(listSome, filename, numOfEntries, pageNumber);
    } else {
        write(STDOUT_FILENO, "Invalid command. Please use 'listSome \"filename\" \"numOfEntries\" \"pageNumber\"'.\n", strlen("Invalid command. Please use 'listSome \"filename\" \"numOfEntries\" \"pageNumber\"'.\n"));
    }
}

void handleShowAll(char* input) {
    char *ptr = input + strlen("showAll ") + 1;
    char *filename = strtok(ptr, "\"");

    if (filename != NULL) {
        showAllInChildProcess(showAll, filename);
    } else {
        write(STDOUT_FILENO, "Invalid command. Please use 'showAll \"filename\"'.\n", strlen("Invalid command. Please use 'showAll \"filename\"'.\n"));
    }
}

int gradeValue(const char *grade) {
    if (strcmp(grade, "AA") == 0) return 8;
    else if (strcmp(grade, "BA") == 0) return 7;
    else if (strcmp(grade, "BB") == 0) return 6;
    else if (strcmp(grade, "CB") == 0) return 5;
    else if (strcmp(grade, "CC") == 0) return 4;
    else if (strcmp(grade, "DC") == 0) return 3;
    else if (strcmp(grade, "DD") == 0) return 2;
    else if (strcmp(grade, "NA") == 0) return 1;
    else if (strcmp(grade, "FF") == 0) return 0;
    return -1;
}

int compare(const void *a, const void *b) {
    Student *studentA = (Student *)a;
    Student *studentB = (Student *)b;
    int result;

    if (sortBy == 0) {
        result = strcmp(studentA->name, studentB->name);
    } else {
        result = gradeValue(studentA->grade) - gradeValue(studentB->grade);
    }

    return sortOrder == 0 ? result : -result;
}

void sortAll(const char *filename, int sortOption, int sortDirection) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        logging("sortAll", "Error opening file");
        exit(EXIT_FAILURE);
    }

    // Count the number of lines in the file
    int count = 0;
    char ch;
    while (read(fd, &ch, 1) > 0) {
        if (ch == '\n') {
            count++;
        }
    }

    // Allocate memory for the students array
    Student *students = malloc(count * sizeof(Student));
    if (students == NULL) {
        perror("Error allocating memory");
        logging("sortAll", "Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // Go back to the start of the file
    lseek(fd, 0, SEEK_SET);

    int i = 0;
    char line[100];
    int index = 0;
    while (read(fd, &ch, 1) > 0) {
        if (ch == '\n') {
            line[index] = '\0'; // Null-terminate the string
            sscanf(line, "\"%[^\"]\" \"%[^\"]\"\n", students[i].name, students[i].grade);
            i++;
            index = 0; // Reset the index for the next line
        } else {
            line[index] = ch;
            index++;
        }
    }

    sortBy = sortOption;
    sortOrder = sortDirection;

    qsort(students, i, sizeof(Student), compare);

    for (int j = 0; j < i; j++) {
        char output[100];
        sprintf(output, "\"%s\" \"%s\"\n", students[j].name, students[j].grade);
        write(STDOUT_FILENO, output, strlen(output));
    }

    close(fd);
    free(students);
}

// Sorts the file in a child process
void sortInChildProcess(void (*func)(const char *, int, int), const char *filename, const char *sortByStr, const char *sortOrderStr) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        logging("sortAll", "fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int sortByInt;
        if (strcmp(sortByStr, "name") == 0) {
            sortByInt = 0;
        } else if (strcmp(sortByStr, "grade") == 0) {
            sortByInt = 1;
        } else {
            perror("Invalid sortBy value. Please use 'name' or 'grade'.");
            logging("sortAll", "Invalid sortBy value. Please use 'name' or 'grade'.");
            exit(EXIT_FAILURE);
        }
        int sortOrderInt;
        if (strcmp(sortOrderStr, "asc") == 0) {
            sortOrderInt = 0;
        } else if (strcmp(sortOrderStr, "desc") == 0) {
            sortOrderInt = 1;
        } else {
            perror("Invalid sortOrder value. Please use 'asc' or 'desc'.");
            logging("sortAll", "Invalid sortOrder value. Please use 'asc' or 'desc'.");
            exit(EXIT_FAILURE);
        }
        func(filename, sortByInt, sortOrderInt);
        exit(EXIT_SUCCESS); // Terminate child process upon successful execution
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            logging("sortAll", "waitpid failed");
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

void listGrades(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        logging("listGrades", "Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;
    ssize_t n;

    write(STDOUT_FILENO, "First 5 entries from ", strlen("First 5 entries from "));
    write(STDOUT_FILENO, filename, strlen(filename));
    write(STDOUT_FILENO, "\n", 1);

    while (count < 5) {
        int i = 0;
        while (i < MAX_LINE_LENGTH - 1) {
            n = read(fd, &line[i], 1);
            if (n == -1) {
                perror("Error reading file");
                logging("listGrades", "Error reading file");
                exit(EXIT_FAILURE);
            } else if (n == 0) {
                break;  // End of file
            }

            if (line[i] == '\n') {
                break;  // End of line
            }

            i++;
        }

        if (n == 0 && i == 0) {
            break;  // End of file
        }

        line[i] = '\0';  // Null-terminate the string
        write(STDOUT_FILENO, line, strlen(line));  // Print the line
        write(STDOUT_FILENO, "\n", 1);
        count++;
    }

    close(fd);
}

// Lists the specified number of entries from the specified page
void listSome(const char *filename, int numOfEntries, int pageNumber) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        logging("listSome", "Error opening file");
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
                write(STDOUT_FILENO, "\n", 1);
                break;
            }
        }
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        logging("listSome", "Error reading file");
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void showAll(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        logging("showAll", "Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;
    int page = 1;
    ssize_t bytesRead;

    write(STDOUT_FILENO, "All entries from ", strlen("All entries from "));

    while ((bytesRead = read(fd, line, sizeof(line))) > 0) {
        for (int i = 0; i < bytesRead; i++) {
            if (line[i] == '\n') {
                count++;
                if (count == 5) {
                    count = 0;
                    page++;
                    write(STDOUT_FILENO, "\nPage ", strlen("Page "));
                    char pageStr[10];
                    sprintf(pageStr, "%d", page);
                    write(STDOUT_FILENO, pageStr, strlen(pageStr));
                    write(STDOUT_FILENO, "\n", 1);

                }
            }
            write(STDOUT_FILENO, &line[i], 1);
        }
    }

    if (bytesRead == -1) {
        perror("Error reading file");
        logging("showAll", "Error reading file");
        exit(EXIT_FAILURE);
    }

    close(fd);
}

