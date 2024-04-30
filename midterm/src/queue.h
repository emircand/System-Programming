#ifndef QUEUE_H
#define QUEUE_H

#include "neHosLib.h"

struct Queue {
    int front, rear, size;
    unsigned capacity;
    struct request** array;
};

struct Queue* createQueue(unsigned capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, struct request* item);
struct request* dequeue(struct Queue* queue);

#endif