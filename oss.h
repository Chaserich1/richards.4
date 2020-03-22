/*  Author: Chase Richards
    Project: Homework 4 CS4760
    Date March 18, 2020
    Filename: oss.h  */

#ifndef OSS_H
#define OSS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>

void displayHelpMessage(); //-h getopt option
void sigHandler(int sig); //Signal Handle(ctrl c and timeout)
void removeAllMem(); //Removes all sharedmemory
FILE* openLogFile(char *file); //Opens the output log file
FILE* filePtr;

void msgqCreation(); //Create message queue

//Shared memory keys and shared memory segment ids
const key_t pcbtKey = 124905;
const key_t clockKey = 120354;
const key_t messageKey = 133139;
int pcbtSegment, clockSegment, msgqSegment;

//Shared memory clock
typedef struct
{
    unsigned int sec;
    unsigned int nanosec;
} clockSim;

//Process control block table
typedef struct
{
    int pid; //0-18 pid
    int priority; //the processes priority
    clockSim cpuTime; //CPU time used
    clockSim sysTime; //Time spent in system
    clockSim waitingTime; //Time waiting   
} pcbTable;

pcbTable *pcbtCreation(int);
#endif
