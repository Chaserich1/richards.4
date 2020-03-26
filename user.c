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
    clockAndTableGetter(procPid);
    pcbTable = pcbtAttach();
    clockSim = clockAttach();       

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

void clockAndTableGetter(int n)
{
    pcbtSegment = shmget(pcbtKey, sizeof(pcbt) * (n + 1), IPC_CREAT | 0777);
    if(pcbtSegment < 0)
    {
        perror("user: Error: Failed to get pcb table segment (shmget)");
        exit(EXIT_FAILURE);
    }

    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0777);
    if(clockSegment < 0)
    {
        perror("user: Error: Failed to get clock segment (shmget)");
        exit(EXIT_FAILURE);
    }
    return;
}

pcbt *pcbtAttach()
{
    pcbt *pcbTable;
    pcbTable = shmat(pcbtSegment, NULL, 0);
    if(pcbTable < 0)
    {
        perror("user: Error: Failed to attach pcb table (shmat)");
        exit(EXIT_FAILURE);
    }
    return pcbTable;
}

clksim *clockAttach()
{
    clksim *clockSim;
    clockSim = shmat(clockSegment, NULL, 0);
    if(clockSim < 0)
    {
        perror("user: Error: Failed to attach clock (shmat)");
        exit(EXIT_FAILURE);
    }
    return clockSim;
}

int determineStatus() {
  int tPercent = 5;   //% chance of terminating
  int bPercent = 5;  //% chance of getting blocked
  int terminating = ((rand() % 100) + 1) <= tPercent ? 1 : 0;
  int blocked = ((rand() % 100) + 1) <= bPercent ? 1 : 0;
  if (terminating) 
    return 1;
  if (blocked) 
    return 2;
  // not blocked or terminating
  return 0;
}
