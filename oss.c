/*  Author: Chase Richards
    Project: Homework 4 CS4760
    Date March 18, 2020
    Filename: oss.c  */

#include "oss.h"
#include "queue.h"

int main(int argc, char* argv[])
{
    int c;
    int n = 18; //Max Children in system at once
    char *outputLog = "ossOutputLog.dat";

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

    //Call function

}

/* Signal handler, that looks to see if the signal is for 2 seconds being up or ctrl-c being entered.
   In both cases, I connect to shared memory so that I can write the time that it is killed to the file
   and so that I can disconnect and remove the shared memory. */
void sigHandler(int sig)
{
    if(sig == SIGALRM)
    {
        //key_t key = ftok(".",'a');
        //int sharedMemSegment;
        //sharedMemSegment = shmget(key, sizeof(struct sharedMemory), IPC_CREAT | 0666);
        //key_t key1 = ftok(".", 'b');
        //int seg;
        //seg = shmget(key1, sizeof(int), IPC_CREAT | 0777);
        printf("Timer is up.\n"); 
        printf("Killing children, removing shared memory and unlinking semaphore.\n");
        //shmctl(sharedMemSegment, IPC_RMID, NULL);
        //shmctl(seg, IPC_RMID, NULL);
        //sem_unlink("semChild");
        //sem_unlink("semLogChild");
        kill(0, SIGKILL);
        exit(EXIT_SUCCESS);
    }
    
    if(sig == SIGINT)
    {
        //key_t key = ftok(".",'a');
        //int sharedMemSegment;
        //sharedMemSegment = shmget(key, sizeof(struct sharedMemory), IPC_CREAT | 0666);
        //key_t key1 = ftok(".", 'b');
        //int seg;
        //seg = shmget(key1, sizeof(int), IPC_CREAT | 0777);
        printf("Ctrl-c was entered\n");
        printf("Killing children, removing shared memory and unlinking semaphore\n");
        //shmctl(sharedMemSegment, IPC_RMID, NULL);
        //shmctl(seg, IPC_RMID, NULL);
        //sem_unlink("semChild");
        //sem_unlink("semLogChild");
        kill(0, SIGKILL);
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
