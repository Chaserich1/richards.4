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

    while((c = getopt(argc, argv, "hn:o:")) != -1)
    {
        switch(c)
        {
            case 'h':
                displayHelpMessage();
                return (EXIT_SUCCESS);
            case 'n':
                n = atoi(optarg);
                break;
            case 'o':
                outputLog = optarg;
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
    int status; 
    int processExec; //exec and check for failure
    pid_t waitingID; 
    int runningProcs = 0;
    int detPriority; //determining the priority of the process
    int availPids[maxProcsInSys];
    int blkedPids[maxProcsInSys];    

    //////////////////////////////////////////
    int schedInc = 1000;
    int idleInc = 100000;
    int blockedInc = 1000000;
    int quantum = 10000000;
    int burst;
    int response;
   
    clksim maxTimeBetweenNewProcesses = {.sec = 0, .nanosec = 500000000};
    clksim nextProc; 
    //////////////////////////////////////////

    questrt *rdrbQueue; //Round robin queue
    rdrbQueue = queueCreation(maxProcsInSys); //Create the round robbin queue  

    pcbt *pcbTable; //Process control block table
    clksim *clockSim; //Simulated clock
    
    pcbTable = pcbtCreation(maxProcsInSys); //Create the pcbt in shared memory
    clockSim = clockCreation(maxProcsInSys); //Create the simed clock in shared memory
    msgqCreation(); //Create the message queue
    
    for(i = 0; i < maxProcsInSys; i++)
    {
        availPids[i] = true;
        blkedPids[i] - false;
    }

    nextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockSim));

    while(procCounter < maxProcs)
    {
        /* If we exceed 10000 lines written then we are finished */
        if(outputLines >= 10000)
            maxProcs = procCounter;            
           
        if(shouldCreateNewProc(maxProcs, procCounter, (*clockSim), nextProc, procPid))
        {
            
            procPid = genProcPid(availPids, maxProcsInSys); //get the available pid (if there is one)            
            
            //if(procPid == -1)
            //{
                char procPidStr[10];
                sprintf(procPidStr, "%d", procPid); //Make the proc pid string for execl  
       
                detPriority = ((rand() % 101) <= 5) ? 0 : 1;             
                availPids[procPid] = false;
 
                pcbTable[procPid] = pcbCreation(detPriority, procPid, (*clockSim));

                enqueue(rdrbQueue, procPid);
                printf("%d\n", rdrbQueue[procPid]);

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
                    processExec = execl("./user", "user", procPidStr, (char *) NULL);
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

                /* if it is a realtime process */
                if(detPriority == 0)
                {
                    //TO DO: Queue the process in rdrbQueue
                    fprintf(filePtr, "rdrbQueue: OSS: Generating process with PID %d and putting it in queue HOLDER at time HOLDER\n", procPid);
                    outputLines++;               
                }
                /* If it is a user process */
                if(detPriority == 1)
                {
                    //TO DO: Queue the process in queue 1 (MLFQ queue)
                    fprintf(filePtr, "OSS: Generating process with PID %d and putting it in queue HOLDER at time HOLDER\n", procPid);
                    outputLines++;
                }
            //}
        }   
        else if((procPid = blockedQueue(blkedPids, pcbTable, maxProcsInSys)) >= 0)
        {
            blkedPids[procPid] = false;
            fprintf(filePtr, "OSS: Unblocked process with PID %d at time HOLDER\n", procPid);
            if(pcbTable[procPid].priority == 0)
            {
                fprintf(filePtr, "OSS: Moving process with PID %d to Round Robin Queue\n");
                enqueue(rdrbQueue, procPid);
            }
            else
            {
                fprintf(filePtr, "OSS: Moving process with PID %d to first queue\n");
                //enqueue in the first queue
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
        }   
 
    }
}

int shouldCreateNewProc(int maxProcs, int procCounter, clksim curTime, clksim nextProcTime, int pid)
{
    if(procCounter >= 17)
        return 0;
    if(pid == -1)
        return 0;
    //if(nextProcTime.sec > curTime.sec)
    //    return 0;
    //if(nextProcTime.sec >= curTime.sec && nextProcTime.nanosec > curTime.nanosec)
    //    return 0;
    return 1;
}

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

//Create pid 1-18
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

int blockedQueue(int *isBlocked, pcbt *pcbTable, int counter)
{
    int i;
    for(i = 0; i < counter; i++)
    {
        if(isBlocked[i] == true)
        {
            if(pcbTable[i].readyToGo == true)
                return i;
        }
    }
    return -1;
}

//////////////////////////////////////
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

///////////////////////////////////////
void clockIncrementor(clksim *simTime, int incrementor)
{
    simTime-> nanosec += incrementor;
    if(simTime-> nanosec >= 1000000000)
    {
        simTime-> sec += 1;
        simTime-> nanosec -= 1000000000;
    }
}

int dispatcher(int fakePid, int priority, int msgId, clksim curTime, int quantum, int *outputLines)
{
    msg message;
    quantum = quantum * pow(2.0, (double)priority);
    message.mType = fakePid + 1;
    message.mValue = quantum;
    fprintf(filePtr, "OSS: Dispatching process with PID %d from queue %d at time HOLDER\n");
    *outputLines++;

    if(msgsnd(msgId, &message, sizeof(message.mValue, 0) == -1))
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

//Create process control table
pcbt *pcbtCreation(int n)
{
    pcbt *pcbtPtr;
    pcbtSegment = shmget(pcbtKey, sizeof(pcbt) * n, IPC_CREAT | 0777);   
    if(pcbtSegment < 0)
    {
        perror("oss: Error: Failed to get process control table segment (shmget)");
        removeAllMem();
    }
    pcbtPtr = shmat(pcbtSegment, NULL, 0);
    if(pcbtPtr < 0)
    {
        perror("oss: Error: Failed to attach to control table segment (shmat)");
        removeAllMem();
    }
}

//Create shared memory clock and initialize time to 0
clksim *clockCreation()
{
    clksim *clockPtr;
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0777);
    if(clockSegment < 0)
    {
        perror("oss: Error: Failed to get clock segment (shmget)");
        removeAllMem();
    }
    clockPtr = shmat(clockSegment, NULL, 0);
    if(clockPtr < 0)
    {
        perror("oss: Error: Failed to attach clock segment (shmat)");
        removeAllMem();
    }
    clockPtr-> sec = 0;
    clockPtr-> nanosec = 0;
    return clockPtr;
}

//Create the message queue
void msgqCreation()
{
    msgqSegment = msgget(messageKey, IPC_CREAT | 0666);
    if(msgqSegment < 0)
    {
        perror("oss: Error: Failed to get message segment (msgget)");
        removeAllMem();
    }
    return;
}

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
        printf("Killing children, removing shared memory and unlinking semaphore.\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
    
    if(sig == SIGINT)
    {
        printf("Ctrl-c was entered\n");
        printf("Killing children, removing shared memory and unlinking semaphore\n");
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
