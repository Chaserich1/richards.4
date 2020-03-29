/*  Author: Chase Richards
    Project: Homework 4 CS4760
    Date March 18, 2020
    Filename: oss.c  */

#include "oss.h"
#include "queue.h"

char *outputLog = "logOutput.dat";

int main(int argc, char* argv[])
{
    int c;
    int n = 18; //Max Children in system at once
    //char *outputLog = "logOutput.dat";
    srand(time(0));
    while((c = getopt(argc, argv, "hn:")) != -1)
    {
        switch(c)
        {
            case 'h':
                displayHelpMessage();
                return (EXIT_SUCCESS);
            case 'n':
                n = atoi(optarg);
                break;
            default:
                printf("Using default values");
                break;               
        }
    }
  
    /* Signal for terminating, freeing up shared mem, killing all children 
       if the program goes for more than two seconds of real clock */
    signal(SIGALRM, sigHandler);
    alarm(20);

    /* Signal for terminating, freeing up shared mem, killing all 
       children if the user enters ctrl-c */
    signal(SIGINT, sigHandler);  

    scheduler(n); //Call scheduler function
    removeAllMem(); //Remove all shared memory, message queue, kill children, close file
    return 0;
}

void scheduler(int maxProcsInSys)
{
    filePtr = openLogFile(outputLog); //open the output file
    
    int maxProcs = 100; //Only 100 processes should be created
    int outputLines = 0; //Counts the lines written to file to make sure we don't have an infinite loop
    int completedProcs = 0; //Processes that have finished
    int procCounter = 0; //Counts the processes
    int procPid; //Generated "fake" Pid (ex. 1,2,3,4,5,etc)
    int realPid; //Generated "real" Pid
    int i = 0; //For loops 
    int processExec; //exec and check for failure
    int detPriority; //determining the priority of the process
    int availPids[maxProcsInSys]; //array of available pids
    int blkedPids[maxProcsInSys]; //array of blocked pids

    char msgqSegmentStr[10];
    char quantumStr[10];


    //////////////////////////////////////////
    int schedInc = 1000;
    int idleInc = 100000;
    int blockedInc = 1000000;
    int quantum = 10000000;
    int burst;
    int response;

    clksim totalCPU = {.sec = 0, .nanosec = 0};
    clksim totalSYS = {.sec = 0, .nanosec = 0};
    clksim totalIdle = {.sec = 0, .nanosec = 0};
    clksim totalWait = {.sec = 0, .nanosec = 0};
    double avgCPU = 0.0;
    double avgSYS = 0.0;
    double avgWait = 0.0;

    sprintf(quantumStr, "%d", quantum);
   
    clksim maxTimeBetweenNewProcesses = {.sec = 0, .nanosec = 500000000};
    clksim nextProc; 
    //////////////////////////////////////////

    questrt *rdrbQueue, *queue1, *queue2, *queue3; //Round robin and mlf queues
    rdrbQueue = queueCreation(maxProcsInSys); //Create the round robbin queue  
    //Create the mlf queue
    queue1 = queueCreation(maxProcsInSys);
    queue2 = queueCreation(maxProcsInSys);
    queue3 = queueCreation(maxProcsInSys);
    
    /* Create process control block shared memory */ 
    pcbt *pcbTable;
    pcbtSegment = shmget(pcbtKey, sizeof(pcbt) * maxProcsInSys, IPC_CREAT | 0666);
    if(pcbtSegment < 0)
    {
        perror("oss: Error: Failed to get process control table segment (shmget)");
        removeAllMem();
    }
    pcbTable = shmat(pcbtSegment, NULL, 0);
    if(pcbTable < 0)
    {
        perror("oss: Error: Failed to attach to control table segment (shmat)");
        removeAllMem();
    }
 
    /* Create the simulated clock in shared memory */
    clksim *clockSim;
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
    if(clockSegment < 0)
    {
        perror("oss: Error: Failed to get clock segment (shmget)");
        removeAllMem();
    }
    clockSim = shmat(clockSegment, NULL, 0);
    if(clockSim < 0)
    {
        perror("oss: Error: Failed to attach clock segment (shmat)");
        removeAllMem();
    }
    clockSim-> sec = 0;
    clockSim-> nanosec = 0;   

    /* Create the message queue */
    msgqSegment = msgget(messageKey, IPC_CREAT | 0777);
    if(msgqSegment < 0)
    {
        perror("oss: Error: Failed to get message segment (msgget)");
        removeAllMem();
    }

    sprintf(msgqSegmentStr, "%d", msgqSegment);
    
    //Set all of the indexes in the available array to 1 and blked array to 0
    for(i = 0; i < maxProcsInSys; i++)
    {
        availPids[i] = 1;
        blkedPids[i] = 0;
    }

    //Start time for the next process is determined by the max time between processes
    nextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockSim));

    //Loop until 100 processes have completed
    while(completedProcs < maxProcs)
    {
        /* If we exceed around 10000 lines written then we break out of the while loop 
           but we still need space for the processes that are out there already */
        if(outputLines >= 9950)
            maxProcs = procCounter;            
    
        procPid = genProcPid(availPids, maxProcsInSys); //get an available pid
        
        if(shouldCreateNewProc(maxProcs, procCounter, (*clockSim), nextProc, procPid))
        {            
            
            char procPidStr[10];
            sprintf(procPidStr, "%d", procPid); //Make the proc pid string for execl  
       
            detPriority = ((rand() % 101) <= 5) ? 0 : 1;             
            availPids[procPid] = 0; //the fake pid is being used so change to zero
 
            pcbTable[procPid] = pcbCreation(detPriority, procPid, (*clockSim));

            /* if it is a realtime process */
            if(detPriority == 0)
            {
                enqueue(rdrbQueue, procPid);
                fprintf(filePtr, "rdrbQueue: OSS: Generating process with PID %d and putting it in queue %d at time %d seconds %d nanoseconds\n", procPid, detPriority, clockSim-> sec, clockSim-> nanosec);
                outputLines++;
            }
            /* If it is a user process */
            if(detPriority == 1)
            {
                enqueue(queue1, procPid);
                fprintf(filePtr, "OSS: Generating process with PID %d and putting it in queue %d at time %d seconds %d nanoseconds\n", procPid, detPriority, clockSim-> sec, clockSim-> nanosec);
                outputLines++;
            }

            //enqueue(rdrbQueue, procPid);
            //printf("%d\n", rdrbQueue[procPid]);

            //Fork the process and check for failure
            realPid = fork(); 
            if(realPid < 0)
            {
                perror("oss: Error: Failed to fork the process");
                removeAllMem();
            }
            else if(realPid == 0)
            {
                //Execl and check for failure
                processExec = execl("./user", "user", procPidStr, msgqSegmentStr, quantumStr, (char *) NULL);
                if(processExec < 0)
                {
                    perror("oss: Error: Failed to execl");
                    removeAllMem();
                }
            }  
 
            procCounter++; //increment the counter
            //runningProcs++; //increment process currently in system   

            nextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockSim));
            clockIncrementor(clockSim, 10000);
            
        }   
        else if((procPid = blockedQueue(blkedPids, pcbTable, maxProcsInSys)) >= 0)
        {
            blkedPids[procPid] = 0;
            fprintf(filePtr, "OSS: Unblocked process with PID %d at time %d seconds %d nanoseconds\n", procPid, clockSim-> sec, clockSim-> nanosec);
            if(pcbTable[procPid].priority == 0)
            {
                fprintf(filePtr, "OSS: Moving process with PID %d to Round Robin Queue\n", procPid);
                enqueue(rdrbQueue, procPid);
            }
            else
            {
                fprintf(filePtr, "OSS: Moving process with PID %d to first queue\n", procPid);
                enqueue(queue1, procPid);
            }
            //increment time
            clockIncrementor(clockSim, blockedInc);           
        }
        else if(rdrbQueue-> items > 0)
        {
            clockIncrementor(clockSim, schedInc);
            procPid = dequeue(rdrbQueue);
            detPriority = pcbTable[procPid].priority;
            response = dispatcher(procPid, detPriority, msgqSegment, (*clockSim), quantum, &outputLines);
            burst = response * (quantum / 100) * pow(2.0, (double)detPriority);
            if(response < 0)
            {
                burst *= -1;
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            else if(response == 100)
            {
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d used full slice and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                fprintf(filePtr, "OSS: Moving process with PID %d to round robin queue\n", procPid);
                enqueue(rdrbQueue, procPid);  
            }
            else
            {
                clockIncrementor(clockSim, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                totalCPU = addTime(totalCPU, pcbTable[procPid].cpuTime);
                totalSYS = addTime(totalSYS, pcbTable[procPid].sysTime);
                totalWait = addTime(totalWait, pcbTable[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            }
        }
        else if(queue1-> items > 0)
        {
            clockIncrementor(clockSim, schedInc);
            procPid = dequeue(queue1);
            detPriority = pcbTable[procPid].priority;
            response = dispatcher(procPid, detPriority, msgqSegment, (*clockSim), quantum, &outputLines);
            burst = response * (quantum / 100) * pow(2.0, (double)detPriority);
            if(response < 0)
            {
                burst *= -1;
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                pcbTable[procPid].priority = 1;
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            else if(response == 100)
            {
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d used full slice and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                pcbTable[procPid].priority = 2;
                fprintf(filePtr, "OSS: Moving process with PID %d to second queue\n", procPid);
                enqueue(queue2, procPid);  
            }
            else
            {
                clockIncrementor(clockSim, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                totalCPU = addTime(totalCPU, pcbTable[procPid].cpuTime);
                totalSYS = addTime(totalSYS, pcbTable[procPid].sysTime);
                totalWait = addTime(totalWait, pcbTable[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            }
        }  
        else if(queue2-> items > 0)
        {
            clockIncrementor(clockSim, schedInc);
            procPid = dequeue(queue2);
            detPriority = pcbTable[procPid].priority;
            response = dispatcher(procPid, detPriority, msgqSegment, (*clockSim), quantum, &outputLines);
            burst = response * (quantum / 100) * pow(2.0, (double)detPriority);
            if(response < 0)
            {
                burst *= -1;
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                pcbTable[procPid].priority = 1;
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            else if(response == 100)
            {
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d used full slice and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                pcbTable[procPid].priority = 3;
                fprintf(filePtr, "OSS: Moving process with PID %d to third queue\n", procPid);
                enqueue(queue3, procPid);  
            }
            else
            {
                clockIncrementor(clockSim, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                totalCPU = addTime(totalCPU, pcbTable[procPid].cpuTime);
                totalSYS = addTime(totalSYS, pcbTable[procPid].sysTime);
                totalWait = addTime(totalWait, pcbTable[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            }
        }
        else if(queue3-> items > 0)
        {
            clockIncrementor(clockSim, schedInc);
            procPid = dequeue(queue3);
            detPriority = pcbTable[procPid].priority;
            response = dispatcher(procPid, detPriority, msgqSegment, (*clockSim), quantum, &outputLines);
            burst = response * (quantum / 100) * pow(2.0, (double)detPriority);
            if(response < 0)
            {
                burst *= -1;
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                pcbTable[procPid].priority = 1;
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            else if(response == 100)
            {
                clockIncrementor(clockSim, burst);
                fprintf(filePtr, "OSS: Process with PID %d used full slice and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                fprintf(filePtr, "OSS: Moving process with PID %d to third queue\n", procPid);
                enqueue(queue3, procPid);  
            }
            else
            {
                clockIncrementor(clockSim, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbTable[procPid].cpuTime, burst);
                pcbTable[procPid].sysTime = subTime((*clockSim), pcbTable[procPid].arrivalTime);
                totalCPU = addTime(totalCPU, pcbTable[procPid].cpuTime);
                totalSYS = addTime(totalSYS, pcbTable[procPid].sysTime);
                totalWait = addTime(totalWait, pcbTable[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            }
        } 
        else
        {
            clockIncrementor(&totalIdle, idleInc);
            clockIncrementor(clockSim, idleInc);
        }
    }
    
    avgCPU = (totalCPU.sec + (.000000001 * totalCPU.nanosec)) / ((double)procCounter);
    printf("Average Throughput: %.2f seconds\n", avgCPU);
    avgSYS = (totalSYS.sec + (.000000001 * totalSYS.nanosec)) / ((double)procCounter);
    printf("Average CPU Time: %.2f seconds\n", avgSYS);
    avgWait = (totalWait.sec + (.000000001 * totalWait.nanosec)) / ((double)procCounter);
    printf("Average Wait Time: %.2f seconds\n", avgWait);
    printf("Total Idle Time: %d.%d seconds\n", totalIdle.sec, totalIdle.nanosec / 10000000);
    printf("Total time of the program: %d.%d seconds\n", clockSim-> sec, clockSim-> nanosec / 10000000);

    return;
}

/* -------------------------------------------------------- */
int shouldCreateNewProc(int maxProcs, int procCounter, clksim curTime, clksim nextProcTime, int pid)
{
    if(procCounter >= maxProcs)
        return 0;
    if(pid < 0)
        return 0;
    if(nextProcTime.sec > curTime.sec)
        return 0;
    if(nextProcTime.sec >= curTime.sec && nextProcTime.nanosec > curTime.nanosec)
        return 0;
    return 1;
}

/* Open the log file that contains the output and check for failure */
FILE *openLogFile(char *file)
{
    filePtr = fopen(file, "a");
    if(filePtr == NULL)
    {
        perror("oss: Error: Failed to open output log file");
        exit(EXIT_FAILURE);
    }
    return filePtr;
}

/* Get proc pid that can go up to 18 but reuse pids that have completed */
int genProcPid(int *pidArr, int totalPids)
{
    int i;
    for(i = 0; i < totalPids; i++)
    {
        if(pidArr[i])
	{
            return i;
        }
    }
    return -1; //Out of pids
}

/* ------------------------------------------------------- */
int blockedQueue(int *isBlocked, pcbt *pcbTable, int counter)
{
    int i;
    for(i = 0; i < counter; i++)
    {
        if(isBlocked[i] == 1)
        {
            if(pcbTable[i].readyToGo == 1)
                return i;
        }
    }
    return -1;
}

/* -------------------------------------------------------- */
clksim nextProcessStartTime(clksim maxTime, clksim curTime)
{
    clksim nextProcTime = {.sec = (rand() % (maxTime.sec + 1)) + curTime.sec,
                           .nanosec = (rand() % (maxTime.nanosec + 1)) + curTime.nanosec};
    if(nextProcTime.nanosec >= 1000000000)
    {
        nextProcTime.sec += 1;
        nextProcTime.nanosec -= 1000000000;
    }
    return nextProcTime;
}

/* -------------------------------------------------------- */
int dispatcher(int fakePid, int priority, int msgId, clksim curTime, int quantum, int *outputLines)
{
    msg message;
    quantum = quantum * pow(2.0, (double)priority);
    message.mType = fakePid + 1;
    message.mValue = quantum;
    fprintf(filePtr, "OSS: Dispatching process with PID %d from queue %d at time %d seconds %d nanoseconds\n", fakePid, priority, curTime.sec, curTime.nanosec);
    *outputLines += 1;

    if(msgsnd(msgId, &message, sizeof(message.mValue), 0) == -1)
    {
        perror("oss: Error: Failed to send the message (msgsnd)");
        removeAllMem();
    }

    if((msgrcv(msgId, &message, sizeof(message.mValue), fakePid + 100, 0)) == -1)
    {
        perror("oss: Error: Failed to receive the message (msgrcv)");
        removeAllMem();
    }
    
    return message.mValue;
}

/* When there is a failure, call this to make sure all memory is removed */
void removeAllMem()
{
    shmctl(pcbtSegment, IPC_RMID, NULL);   
    shmctl(clockSegment, IPC_RMID, NULL);
    msgctl(msgqSegment, IPC_RMID, NULL);
    fclose(filePtr);
} 

/* Signal handler, that looks to see if the signal is for 2 seconds being up or ctrl-c being entered.
   In both cases, I connect to shared memory so that I can write the time that it is killed to the file
   and so that I can disconnect and remove the shared memory. */
void sigHandler(int sig)
{
    if(sig == SIGALRM)
    {
        printf("Timer is up.\n"); 
        printf("Killing children, removing shared memory and message queue.\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
    
    if(sig == SIGINT)
    {
        printf("Ctrl-c was entered\n");
        printf("Killing children, removing shared memory and message queue\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
}


/* For the -h option that can be entered */
void displayHelpMessage() 
{
    printf("\n---------------------------------------------------------\n");
    printf("See below for the options:\n\n");
    printf("-h    : Instructions for running the project and terminate.\n"); 
    printf("-n x  : Number of processes allowed in the system at once (Default: 18).\n");
    printf("-o filename  : Option to specify the output log file name (Default: ossOutputLog.dat).\n");
    printf("\n---------------------------------------------------------\n");
    printf("Examples of how to run program(default and with options):\n\n");
    printf("$ make\n");
    printf("$ ./oss\n");
    printf("$ ./oss -n 10 -o outputLog.dat\n");
    printf("$ make clean\n");
    printf("\n---------------------------------------------------------\n"); 
    exit(0);
}
