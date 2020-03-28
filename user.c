/*  Author: Chase Richards
    Project: Homework 4 CS4760
    Date March 18, 2020
    Filename: user.c  */

#include "oss.h"

int main(int argc, char *argv[])
{
    int procPid;
    msg message;
    int quantum;
    int status = 0;
    procPid = atoi(argv[1]);
    msgqSegment = atoi(argv[2]);
    quantum = atoi(argv[3]);
    srand(time(0) + (procPid + 1));

    pcbt *pcbTable;
    clksim *clockSim;
    clksim timeBlocked;
    clksim event;
    int burst;
    
    pcbtSegment = shmget(pcbtKey, sizeof(pcbt) * (procPid + 1), IPC_CREAT | 0666);
    if(pcbtSegment < 0)
    {
        perror("user: Error: Failed to get pcb table segment (shmget)");
        exit(EXIT_FAILURE);
    }
;
    pcbTable = shmat(pcbtSegment, NULL, 0);
    if(pcbTable < 0)
    {
        perror("user: Error: Failed to attach pcb table (shmat)");
        exit(EXIT_FAILURE);
    }
    
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
    if(clockSegment < 0)
    {
        perror("user: Error: Failed to get clock segment (shmget)");
        exit(EXIT_FAILURE);
    }

    clockSim = shmat(clockSegment, NULL, 0);
    if(clockSim < 0)
    {
        perror("user: Error: Failed to attach clock (shmat)");
        exit(EXIT_FAILURE);
    }       

    status = determineStatus();

    while (status != 1)
    {
        if((msgrcv(msgqSegment, &message, sizeof(message.mValue), (procPid + 1), 0)) == -1)
        {
            perror("user: Error: Failed to recieve message (msgrcv)");
            exit(EXIT_FAILURE);
        }
 
        status = determineStatus();
        switch(status)
        {
            case 0:
                message.mValue = 100;
                break;
            case 1:
                message.mValue = (rand() % 99) + 1;
                break;
            case 2:
                message.mValue = ((rand() % 99) + 1) * -1;
                timeBlocked.nanosec = clockSim-> nanosec;
                timeBlocked.sec = clockSim-> sec;
                burst = message.mValue * (quantum / 100) * pow(2.0, (double)pcbTable[procPid].priority);
                event.nanosec = (rand() % 1000) * 1000000;
                event.sec = (rand() % 2) + 1;
                pcbTable[procPid].waitingTime = addTime(pcbTable[procPid].waitingTime, event);
                event = addTime(event, timeBlocked);
                clockIncrementor(&event, (burst * -1));
                pcbTable[procPid].readyToGo = 0;
                break;
            default:
                break;
        }

        message.mType = procPid + 100;
        
        if(msgsnd(msgqSegment, &message, sizeof(message.mValue), 0) == -1)
        {
            perror("user: Error: Failed to send message (msgsnd)");
            exit(EXIT_FAILURE);
        }

        if(status == 2)
        {
            while(pcbTable[procPid].readyToGo == 0)
            {
                if(event.sec > clockSim-> sec)
                {
                    pcbTable[procPid].readyToGo = 1;
                }
                else if(event.nanosec >= clockSim-> nanosec && event.sec >= clockSim-> sec)
                {
                    pcbTable[procPid].readyToGo = 1;
                }
            }
        } 

    }

    return 0;
}

int determineStatus()
{
    int timeToTerminate = ((rand() % 100) + 1) <= 5 ? 1 : 0;
    int timeToBlock = ((rand() % 100) + 1) <= 5 ? 1 : 0;
    if(timeToTerminate)
        return 1;
    else if(timeToBlock)
        return 2;
    else
        return 0;
}
