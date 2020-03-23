/*  Author: Chase Richards
    Project: Homework 4 CS4760
    Date March 18, 2020
    Filename: queue.h  */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

typedef struct
{
    unsigned int capacity;
    unsigned int items, front, back;
    int *arr;
} questrt;

questrt *queueCreation(int capacity)
{
    int i;
    questrt *queue = (questrt *)malloc(sizeof(questrt));
    queue-> items = 0;
    queue-> front = 0;
    queue-> back = 0;
    queue-> capacity = capacity;
    queue-> arr = (int *)malloc(capacity * sizeof(int));
    for(i = 0; i < capacity; i++)
        queue-> arr[i] = -1;
    return queue;
}

void enqueue(questrt *queue, int pid)
{
    queue-> items += 1;
    queue-> arr[queue-> back] = pid;
    queue-> back = (queue-> back + 1) % queue-> capacity;
}

int dequeue(questrt *queue)
{
    int pid;
    queue-> items -= 1;
    pid = queue-> arr[queue-> front];
    queue-> front = (queue-> front + 1) % queue-> capacity;
    return pid;
}

#endif
