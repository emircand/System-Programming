#include "sortList.h"
#include "hw01.h"

void sortInChildProcess(void (*func)(const char *, int, int), const char *filename, const char *sortBy, const char *sortOrder) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int sortByInt;
        if (strcmp(sortBy, "name") == 0) {
            sortByInt = 0;
        } else if (strcmp(sortBy, "grade") == 0) {
            sortByInt = 1;
        } else {
            perror("Invalid sortBy value. Please use 'name' or 'grade'.");
            exit(EXIT_FAILURE);
        }
        int sortOrderInt;
        if (strcmp(sortOrder, "asc") == 0) {
            sortOrderInt = 0;
        } else if (strcmp(sortOrder, "desc") == 0) {
            sortOrderInt = 1;
        } else {
            perror("Invalid sortOrder value. Please use 'asc' or 'desc'.");
            exit(EXIT_FAILURE);
        }
        func(filename, sortByInt, sortOrderInt);
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

void listGradesInChildProcess(void (*func)(const char *), const char *filename) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
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

void listSomeInChildProcess(void (*func)(const char *, int, int), const char *filename, const char *numOfEntries, const char *pageNumber) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
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

void showAllInChildProcess(void (*func)(const char *), const char *filename) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
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

void sortAll(const char *filename, int sortBy, int sortOrder) {
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

    // Print sorted lines to the terminal
    for (int i = 0; i < numLines; i++) {
        write(STDOUT_FILENO, lines[i], strlen(lines[i]));
        write(STDOUT_FILENO, "\n", 1); // Add a newline character after each line
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

int gradeValue(const char *grade) {
    if (strcmp(grade, "AA") == 0) return 8;
    else if (strcmp(grade, "BA") == 0) return 7;
    else if (strcmp(grade, "BB") == 0) return 6;
    else if (strcmp(grade, "CB") == 0) return 5;
    else if (strcmp(grade, "CC") == 0) return 4;
    else if (strcmp(grade, "DC") == 0) return 3;
    else if (strcmp(grade, "DD") == 0) return 2;
    else if (strcmp(grade, "FD") == 0) return 1;
    else if (strcmp(grade, "NA") == 0) return 0;
    else if (strcmp(grade, "FF") == 0) return 0;
    else if (strcmp(grade, "VF") == 0) return 0;
    return 0;
}

int compareByGradeAsc(const void *a, const void *b) {
    const char *gradeA = *(const char **)a;
    const char *gradeB = *(const char **)b;
    return gradeValue(gradeA) - gradeValue(gradeB);
}

int compareByGradeDesc(const void *a, const void *b) {
    const char *gradeA = *(const char **)a;
    const char *gradeB = *(const char **)b;
    return gradeValue(gradeB) - gradeValue(gradeA);
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

