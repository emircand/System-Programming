#ifndef sortList_h
#define sortList_h


int compareByNameAsc(const void *a, const void *b);
int compareByNameDesc(const void *a, const void *b);
int compareByGradeAsc(const void *a, const void *b);
int compareByGradeDesc(const void *a, const void *b);

void sortAll(const char *filename, int sortBy, int sortOrder);
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


#endif /* sortList_h */