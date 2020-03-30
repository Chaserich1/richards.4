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
    alarm(3);

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
    int priority; //determining the priority of the process
    int availPids[maxProcsInSys]; //bit array of available pids
    int blkedPids[maxProcsInSys]; //bit array of blocked pids

    char msgqSegmentStr[10];
    char quantumStr[10];

    int schedInc = 1000; //Scheduler increment for time
    int idleInc = 100000; //Increment when no procs are ready
    int blockedInc = 1000000; //Increment for checking the blocked queue
    int quantum = 10000000; //10 milliseconds quantum
    int burst; //burst tiume
    int receivedMsg; //Recieved from child telling scheduler what to do

    /* Stats for the end of the program */
    clksim cpuTotal = {.sec = 0, .nanosec = 0};
    clksim tpTotal = {.sec = 0, .nanosec = 0};
    clksim blkedTotal = {.sec = 0, .nanosec = 0};
    clksim idleTotal = {.sec = 0, .nanosec = 0};
    clksim waitingTotal = {.sec = 0, .nanosec = 0};
    double cpuAvg = 0.0;
    double tpAverage = 0.0;
    double blkedAvg = 0.0;
    double waitingAvg = 0.0;

    sprintf(quantumStr, "%d", quantum);

    //Constant for the time between each new process   
    clksim maxTimeBetweenNewProcesses = {.sec = 0, .nanosec = 900000000};
    clksim nextProc; 

    questrt *rdrbQueue, *queue1, *queue2, *queue3; //Round robin and mlf queues
    rdrbQueue = queueCreation(maxProcsInSys); //Create the round robbin queue  
    //Create the mlf queue
    queue1 = queueCreation(maxProcsInSys);
    queue2 = queueCreation(maxProcsInSys);
    queue3 = queueCreation(maxProcsInSys);
    
    /* Create process control block shared memory */ 
    pcbt *pcbtPtr;
    pcbtSegment = shmget(pcbtKey, sizeof(pcbt) * maxProcsInSys, IPC_CREAT | 0666);
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
 
    /* Create the simulated clock in shared memory */
    clksim *clockPtr;
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
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
    nextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockPtr));

    //Loop until 100 processes have completed
    while(completedProcs < maxProcs)
    {
        /* Advance the clock by 1.xx where xx is random between [1,1000]*/
        int loopInc = (rand() % (1000 - 1 + 1));
        clockIncrementor(clockPtr, loopInc);        

        /* If we exceed around 10000 lines written then we break out of the while loop 
           but we still need space for the processes that are out there already */
        if(outputLines >= 9950)
            maxProcs = procCounter;            
    
        procPid = genProcPid(availPids, maxProcsInSys); //get an available pid
        
        if(shouldCreateNewProc(maxProcs, procCounter, (*clockPtr), nextProc, procPid))
        {            
            
            char procPidStr[10];
            sprintf(procPidStr, "%d", procPid); //Make the proc pid string for execl  
       
            /* User process or real-time process, note it is heavily
               weighted to be a user process */
            priority = ((rand() % 101) <= 5) ? 0 : 1;             
            availPids[procPid] = 0; //the fake pid is being used so change to zero
            
            //Allocate and initialize the process control block before forking
            pcbtPtr[procPid] = pcbCreation(priority, procPid, (*clockPtr));

            /* if it is a realtime process enqueue in the round robin queue and specify that to output file */
            if(priority == 0)
            {
                enqueue(rdrbQueue, procPid);
                fprintf(filePtr, "OSS: Generating process with PID %d and putting it in red robin queue at time %d:%d\n", procPid, clockPtr-> sec, clockPtr-> nanosec);
                outputLines++;
            }
            /* If it is a user process enqueue in queue 1 of the mlf queue and specify that to output file*/
            if(priority == 1)
            {
                enqueue(queue1, procPid);
                fprintf(filePtr, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%d\n", procPid, priority, clockPtr-> sec, clockPtr-> nanosec);
                outputLines++;
            }

            /* This was for testing */
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

            //Get the start time for the next process based on the maxtime betweeen processes and increment the clock
            nextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockPtr));
            clockIncrementor(clockPtr, 10000);
            
        }
        /* If the process pid is blocked then unblock it and move it to either the
           round robin queue or mlf queue depending on it's priority in shared mem struct */   
        else if((procPid = blockedQueue(blkedPids, pcbtPtr, maxProcsInSys)) >= 0)
        {
            blkedPids[procPid] = 0;
            fprintf(filePtr, "OSS: Unblocked process with PID %d at time %d:%d\n", procPid, clockPtr-> sec, clockPtr-> nanosec);
            if(pcbtPtr[procPid].priority == 0)
            {
                fprintf(filePtr, "OSS: Moving process with PID %d to Round Robin Queue\n", procPid);
                enqueue(rdrbQueue, procPid);
            }
            else
            {
                fprintf(filePtr, "OSS: Moving process with PID %d to first queue\n", procPid);
                enqueue(queue1, procPid);
            }
            //increment time for the checking of the blocked queue and assign to total blocked time
            clockIncrementor(clockPtr, blockedInc);
            pcbtPtr[procPid].blkedTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
            blkedTotal = addTime(blkedTotal, pcbtPtr[procPid].blkedTime);         
        }
        /* Round Robin queue for the real-time processes */
        else if(rdrbQueue-> items > 0)
        {
            clockIncrementor(clockPtr, schedInc); //Overhead for scheduling
            procPid = dequeue(rdrbQueue); //dequeue the process
            priority = pcbtPtr[procPid].priority; //get the priority from sm
            //Dispatch the process message and find the burst time
            receivedMsg = dispatcher(procPid, priority, msgqSegment, (*clockPtr), quantum, &outputLines);
            burst = receivedMsg * (quantum / 100) * pow(2.0, (double)priority);
            /* The process is blocked and moves to the blocked "queue" */
            if(receivedMsg < 0)
            {
                //Increment the clock based on the time burst
                burst *= -1;
                clockIncrementor(clockPtr, burst); 
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst); //increment the time used in burst
                //the throughput time is the current time minus the arrival time of the process
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime); 
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1; //this pid is now showing blocked in the blocked array
            }
            /* The process is neither blocked or terminated, it used its time */
            else if(receivedMsg == 100)
            {
                clockIncrementor(clockPtr, burst); //increment the clock with the burst
                fprintf(filePtr, "OSS: Recieving that process with PID %d ran for %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst); //icnrement the clock
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime); //get the tp time
                fprintf(filePtr, "OSS: Moving process with PID %d to round robin queue\n", procPid);
                enqueue(rdrbQueue, procPid); //enqueue in the round robin queue because real-time stays real-time its whole life
            }
            /* The process is to terminate so the pid opens up again and stats are calculated */
            else
            {
                clockIncrementor(clockPtr, burst); //icrement the clock
                realPid = wait(NULL); //wait for the process
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst); //increment
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime); //get the tp time
                //Update the CPU, throughput, waiting time totals
                cpuTotal = addTime(cpuTotal, pcbtPtr[procPid].cpuTime);
                tpTotal = addTime(tpTotal, pcbtPtr[procPid].tpTime);
                waitingTotal = addTime(waitingTotal, pcbtPtr[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1; //This pid is now available
                completedProcs += 1; //incrment the number of procs completed
            }
        }
        /* Start of the Mlf queue but now when the process uses it's time then it gets moved down
           to queue2 */
        else if(queue1-> items > 0)
        {
            clockIncrementor(clockPtr, schedInc);
            procPid = dequeue(queue1);
            priority = pcbtPtr[procPid].priority;
            receivedMsg = dispatcher(procPid, priority, msgqSegment, (*clockPtr), quantum, &outputLines);
            burst = receivedMsg * (quantum / 100) * pow(2.0, (double)priority);
            /* The process is blocked and moves to the blocked "queue" */
            if(receivedMsg < 0)
            {
                burst *= -1;
                clockIncrementor(clockPtr, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                pcbtPtr[procPid].priority = 1;
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            /* The process is neither blocked or terminated, it is active */
            else if(receivedMsg == 100)
            {
                clockIncrementor(clockPtr, burst);
                fprintf(filePtr, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                pcbtPtr[procPid].priority = 2;
                fprintf(filePtr, "OSS: Moving process with PID %d to second queue\n", procPid);
                enqueue(queue2, procPid);  
            }
            /* The process is to terminate so the pid opens up again and stats are calculated */
            else
            {
                clockIncrementor(clockPtr, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                cpuTotal = addTime(cpuTotal, pcbtPtr[procPid].cpuTime);
                tpTotal = addTime(tpTotal, pcbtPtr[procPid].tpTime);
                waitingTotal = addTime(waitingTotal, pcbtPtr[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            }
        }
        /* When a process uses it's time then it is moved to queue 3 */  
        else if(queue2-> items > 0)
        {
            clockIncrementor(clockPtr, schedInc);
            procPid = dequeue(queue2);
            priority = pcbtPtr[procPid].priority;
            receivedMsg = dispatcher(procPid, priority, msgqSegment, (*clockPtr), quantum, &outputLines);
            burst = receivedMsg * (quantum / 100) * pow(2.0, (double)priority);
            /* The process is blocked and moves to the blocked "queue" */
            if(receivedMsg < 0)
            {
                burst *= -1;
                clockIncrementor(clockPtr, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                pcbtPtr[procPid].priority = 1;
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            /* The process is neither blocked or terminated, it is active */
            else if(receivedMsg == 100)
            {
                clockIncrementor(clockPtr, burst);
                fprintf(filePtr, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                pcbtPtr[procPid].priority = 3;
                fprintf(filePtr, "OSS: Moving process with PID %d to third queue\n", procPid);
                enqueue(queue3, procPid);  
            }
            /* The process is to terminate so the pid opens up again and stats are calculated */
            else
            {
                clockIncrementor(clockPtr, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                cpuTotal = addTime(cpuTotal, pcbtPtr[procPid].cpuTime);
                tpTotal = addTime(tpTotal, pcbtPtr[procPid].tpTime);
                waitingTotal = addTime(waitingTotal, pcbtPtr[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            } 
        }
        /* When a process uses its time it goes back into queue3 again */   
        else if(queue3-> items > 0)
        {
            clockIncrementor(clockPtr, schedInc);
            procPid = dequeue(queue3);
            priority = pcbtPtr[procPid].priority;
            receivedMsg = dispatcher(procPid, priority, msgqSegment, (*clockPtr), quantum, &outputLines);
            burst = receivedMsg * (quantum / 100) * pow(2.0, (double)priority);
            /* The process is blocked and moves to the blocked "queue" */
            if(receivedMsg < 0)
            {
                burst *= -1;
                clockIncrementor(clockPtr, burst);
                fprintf(filePtr, "OSS: Process with PID %d is blocked and used %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                pcbtPtr[procPid].priority = 1;
                fprintf(filePtr, "OSS: Moving process with PID %d to blocked queue\n", procPid);
                blkedPids[procPid] = 1;
            }
            /* The process is neither blocked or terminated, it is active */
            else if(receivedMsg == 100)
            {
                clockIncrementor(clockPtr, burst);
                fprintf(filePtr, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", procPid, burst);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                fprintf(filePtr, "OSS: Moving process with PID %d to third queue\n", procPid);
                enqueue(queue3, procPid);  
            }
            /* The process is to terminate so the pid opens up again and stats are calculated */
            else
            {
                clockIncrementor(clockPtr, burst);
                realPid = wait(NULL);
                clockIncrementor(&pcbtPtr[procPid].cpuTime, burst);
                pcbtPtr[procPid].tpTime = subTime((*clockPtr), pcbtPtr[procPid].arrivalTime);
                cpuTotal = addTime(cpuTotal, pcbtPtr[procPid].cpuTime);
                tpTotal = addTime(tpTotal, pcbtPtr[procPid].tpTime);
                waitingTotal = addTime(waitingTotal, pcbtPtr[procPid].waitingTime);
                fprintf(filePtr, "OSS: Process with PID %d terminated and used %d nanoseconds\n", procPid, burst);
                availPids[procPid] = 1;
                completedProcs += 1;
            }
        } 
        /* The program is idle with no ready processes */
        else
        {
            clockIncrementor(&idleTotal, idleInc);
            clockIncrementor(clockPtr, idleInc);
        }
    }
    
    /* Print the final stats to the screen including: avg wait time, avg CPU util,
       avg waiting time in a blocked queue, and how long the CPU was idle */
    waitingAvg = (waitingTotal.sec + (.000000001 * waitingTotal.nanosec)) / ((double)procCounter);
    printf("Average Wait Time: %.2f seconds\n", waitingAvg); 
    cpuAvg = (cpuTotal.sec + (.000000001 * cpuTotal.nanosec)) / ((double)procCounter);
    printf("Average CPU Utilization: %.2f seconds\n", cpuAvg);
    //tpAverage = (tpTotal.sec + (.000000001 * tpTotal.nanosec)) / ((double)procCounter);
    //printf("Average Throughput Time: %.2f seconds\n", tpAverage);
    blkedAvg = (blkedTotal.sec + (.000000001 * blkedTotal.nanosec)) / ((double)procCounter);
    printf("Average Blocked Time: %.2f seconds\n", blkedAvg);
    printf("Total Idle Time with no ready processes: %d.%d seconds\n", idleTotal.sec, idleTotal.nanosec / 10000000);
    printf("Total time of the program: %d.%d seconds\n", clockPtr-> sec, clockPtr-> nanosec / 10000000);

    return;
}

/* Determine if a new process should be created based on the conditions below */
int shouldCreateNewProc(int maxProcs, int procCounter, clksim curTime, clksim nextProcTime, int pid)
{
    if(procCounter >= maxProcs) //less than 100
        return 0;
    if(pid < 0) //pid is available
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

/* If the process is blocked and the process is ready then return it to unblock it */
int blockedQueue(int *isBlocked, pcbt *pcbtPtr, int counter)
{
    int i;
    for(i = 0; i < counter; i++)
    {
        if(isBlocked[i] == 1)
        {
            if(pcbtPtr[i].readyToGo == 1)
                return i;
        }
    }
    return -1;
}

/* Determines when to launch the next process based on a random value
   between 0 and maxTimeBetweenNewProcs and returns the time that it should launch */
clksim nextProcessStartTime(clksim maxTime, clksim curTime)
{
    clksim nextProcTime = {.sec = (rand() % (maxTime.sec + 1)) + curTime.sec, .nanosec = (rand() % (maxTime.nanosec + 1)) + curTime.nanosec};
    if(nextProcTime.nanosec >= 1000000000)
    {
        nextProcTime.sec += 1;
        nextProcTime.nanosec -= 1000000000;
    }
    return nextProcTime;
}

/* Sends a message with the time quantum to the pid and waits for a receivedMsgs to return whether it
   is blocked, terminating, or neither */
int dispatcher(int fakePid, int priority, int msgId, clksim curTime, int quantum, int *outputLines)
{
    msg message;
    quantum = quantum * pow(2.0, (double)priority);
    message.mType = fakePid + 1;
    message.mValue = quantum;
    fprintf(filePtr, "OSS: Dispatching process with PID %d from queue %d at time %d:%d\n", fakePid, priority, curTime.sec, curTime.nanosec);
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
