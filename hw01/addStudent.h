#ifndef addStudent_h
#define addStudent_h

#include <stdio.h>

void addStudentGrade(const char *filename, const char *studentName, const char *grade);
void addInChildProcess(void (*func)(const char *, const char *, const char *), const char *filename, const char *studentName, const char *grade);
void handleAddStudentGrade(char* input);


#endif /* addStudent_h */