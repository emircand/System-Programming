#ifndef searchStudent_h
#define searchStudent_h

void searchStudent(const char *filename, const char *studentName);
void handleSearchStudent(char* input);
void searchInChildProcess(void (*func)(const char *, const char *), const char *filename, const char *studentName);

#endif /* searchStudent_h */