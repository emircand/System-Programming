#ifndef sortList_h
#define sortList_h

void sortAll(const char *filename, int sortOption, int sortDirection);
void showAll(const char *filename);
void listGrades(const char *filename);
void listSome(const char *filename, int numOfEntries, int pageNumber);

void handleSortAll(char* input);
void handleListGrades(char* input);
void handleListSome(char* input);
void handleShowAll(char* input);

void sortInChildProcess(void (*func)(const char *, int, int), const char *filename, const char *sortBy, const char *sortOrder);
void listGradesInChildProcess(void (*func)(const char *), const char *filename);
void listSomeInChildProcess(void (*func)(const char *, int, int), const char *filename, const char *numOfEntries, const char *pageNumber);
void showAllInChildProcess(void (*func)(const char *), const char *filename);

int compare(const void *a, const void *b);
int gradeValue(const char *grade);

#endif /* sortList_h */