/**
 * @file child.c
 * @brief This file contains the implementation of a child process that interacts with other child processes and a parent process using signals.
 *
 * The child process reads its index and the number of child processes from command line arguments and a file respectively.
 * It then enters an infinite loop where it waits for signals and handles them accordingly.
 *
 * The following signals are handled:
 * - SIGUSR1: Used to print the status of the child process and propagate the signal to the next child process.
 * - SIGUSR2: Used to determine if the child process catches or misses a signal based on a random probability.
 * - SIGINT: Used to terminate the child process and declare it as the winner if it is in the default state.
 *
 * The child process also reads the PIDs of other child processes from a file and stores them in an array.
 * It uses these PIDs to send signals to other child processes.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments.
 *
 * @return Returns 0 on successful execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <string.h>

int n;
pid_t *child_pids;
int my_index;
int mystatus = 1;

void print_status()
{
    if(mystatus == 1){
        printf("  ....  ");
    }else if(mystatus == 2){
        printf("  CATCH ");
        mystatus = 1;
    }else if(mystatus == 3){
         printf("  MISS  ");
        mystatus = 4;
    }else if(mystatus == 4){
        printf("        ");
    }
    fflush(stdout);
}

void executeDummy()
{
    FILE *fp = fopen("dummycpid.txt", "r");
    if (fp)
    {
        pid_t dummy_pid;
        if (fscanf(fp, "%d", &dummy_pid) == 1)
        {
            kill(dummy_pid, SIGINT);
        }
        fclose(fp);
    }
}

void signal_handler(int signum)
{
    if (signum == SIGUSR2)
    {
        if (mystatus == 1)
        {
            if ((double)rand() / RAND_MAX < 0.8)
            {
                mystatus = 2;

                kill(getppid(), SIGUSR1);
            }
            else
            {
                mystatus = 3;

                kill(getppid(), SIGUSR2);
            }
        }
    }
    else if (signum == SIGUSR1)
    {
        if (my_index == 1)
        {
            printf("|");
        }
        print_status();
        if (my_index == n)
        {
            printf("|\n");
            executeDummy();
        }
        else
        {
            kill(child_pids[my_index], SIGUSR1);
        }
    }
    else if (signum == SIGINT)
    {
        if (mystatus == 1)
        {
            printf("+++ Child %d: Yay! I am the winner!\n", my_index);
            fflush(stdout);
        }
        free(child_pids);
        exit(0);
    }
    signal(signum, signal_handler);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <integer>\n", argv[0]);
        return 1;
    }
    my_index = atoi(argv[1]);
    srand(time(NULL) ^ (my_index * 1000));
    sleep(1);

    FILE *fp = fopen("childpid.txt", "r");
    if (!fp)
    {
        fprintf(stderr, "Failed to open childpid.txt\n");
        exit(1);
    }
    if (fscanf(fp, "%d", &n) != 1)
    {
        perror("Failed to read childpid.txt");
        fclose(fp);
        exit(1);
    }

    child_pids = (pid_t *)malloc(n * sizeof(pid_t));

    for (int i = 0; i < n; i++)
    {
        if (fscanf(fp, "%d", &child_pids[i]) != 1)
        {
            perror("Failed to read childpid.txt");
            fclose(fp);
            exit(1);
        }
    }
    fclose(fp);

    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGINT, signal_handler);

    while (1)
    {
        pause();
    }

    return 0;
}