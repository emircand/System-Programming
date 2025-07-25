#include "neHosLib.h"
#include "queue.h"

struct Queue* createQueue(unsigned capacity) {
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (struct request**) malloc(queue->capacity * sizeof(struct request*));
    return queue;
}

int isFull(struct Queue* queue) {
    return (queue->size == queue->capacity);
}

int isEmpty(struct Queue* queue) {
    return (queue->size == 0);
}

void enqueue(struct Queue* queue, struct request* item) {
    if (isFull(queue)) {
        return;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size++;
}

struct request* dequeue(struct Queue* queue) {
    if (isEmpty(queue)) {
        return NULL;
    }
    struct request* item = queue->array[queue->front];
    queue->array[queue->front] = NULL;
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return item;
}

void resizeQueue(struct Queue* queue, unsigned newCapacity) {
    struct request** newArray = (struct request**) malloc(newCapacity * sizeof(struct request*));
    unsigned i = 0;
    while (!isEmpty(queue)) {
        newArray[i++] = dequeue(queue);
    }
    free(queue->array);
    queue->array = newArray;
    queue->capacity = newCapacity;
    queue->front = 0;
    queue->rear = i - 1;
    queue->size = i;
}

void destroyQueue(struct Queue* queue) {
    if (queue == NULL) {
        return;
    }

    // Free each request in the queue
    for (int i = queue->front; i != queue->rear; i = (i + 1) % queue->capacity) {
        free(queue->array[i]);
    }

    // Free the array of requests
    free(queue->array);

    // Free the queue itself
    free(queue);
}