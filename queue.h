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
    unsigned int items;
    unsigned int front;
    unsigned int back;
    int *arr;
} questrt;

/* Create the queue */
questrt *queueCreation(int capacity)
{
    int i;
    questrt *queuePtr = (questrt *)malloc(sizeof(questrt));
    queuePtr-> items = 0;
    queuePtr-> front = 0;
    queuePtr-> back = 0;
    queuePtr-> capacity = capacity;
    queuePtr-> arr = (int *)malloc(capacity * sizeof(int));
    for(i = 0; i < capacity; i++)
        queuePtr-> arr[i] = -1;
    return queuePtr;
}

/* Add a process to the queue */
void enqueue(questrt *queuePtr, int pid)
{
    queuePtr-> items += 1;
    queuePtr-> arr[queuePtr-> back] = pid;
    queuePtr-> back = (queuePtr-> back + 1) % queuePtr-> capacity;
    return;
}

/* Remove a process from the queue */
int dequeue(questrt *queuePtr)
{
    int pid;
    queuePtr-> items -= 1;
    pid = queuePtr-> arr[queuePtr-> front];
    queuePtr-> front = (queuePtr-> front + 1) % queuePtr-> capacity;
    return pid;
}

#endif
