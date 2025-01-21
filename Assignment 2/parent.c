/**
 * @file parent.c
 * @brief This file contains the implementation of the parent process that manages a game involving multiple child processes.
 * 
 * The parent process creates a specified number of child processes and coordinates their actions using signals.
 * The game continues until only one child process remains active.
 * 
 * The parent process performs the following tasks:
 * - Creates child processes and writes their PIDs to a file.
 * - Sets up signal handlers to manage communication with child processes.
 * - Starts a dummy process to simulate some activity.
 * - Coordinates the game by sending signals to child processes and handling their responses.
 * - Terminates all child processes when the game ends.
 * 
 * The child processes are expected to execute a separate program ("child") and communicate with the parent process using signals.
 * 
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments. The first argument is the program name, and the second argument is the number of players.
 * 
 * @return Returns 0 on successful execution.
 * 
 * @note The program expects the "child" and "dummy" executables to be present in the same directory.
 * @note The program writes the PIDs of the child processes to "childpid.txt" and the PID of the dummy process to "dummycpid.txt".
 * 
 * @dependencies
 * - stdio.h
 * - stdlib.h
 * - unistd.h
 * - signal.h
 * - sys/types.h
 * - sys/wait.h
 * - stdbool.h
 * - string.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>

volatile sig_atomic_t signal_recieved = 0;
int n;
int *child_pids;
pid_t dummy_pid;
int *child_status;

void formatprint()
{

    for (int i = 1; i <= n; i++)
    {
        if (i == 1)
            printf("+");
        printf("--------");
        if (i == n)
            printf("\n");
    }
    for (int i = 1; i <= n; i++)
    {
        printf("    %d   ", i);
        if (i == n)
            printf("\n");
    }
}
void signal_usr1(int signum)
{
    signal_recieved = 1;
}
void signal_usr2(int signum)
{
    signal_recieved = 2;
}

void signalHandler(int signum)
{
    if (signum == SIGUSR1)
    {
        signal_usr1(signum);
    }
    else if (signum == SIGUSR2)
    {
        signal_usr2(signum);
    }
}
int find_next_player(int current, int n, int *child_status)
{
    int next_player = (current + 1) % n;
    while (child_status[next_player] != 0)
    {
        next_player = (next_player + 1) % n;
        if (next_player == current)
            return current;
    }
    return next_player;
}

int count_playing_players(int n, int *child_status)
{
    int count = 0;
    for (int i = 0; i < n; i++)
    {
        if (child_status[i] == 0)
            count++;
    }
    return count;
}

void startDummycode()
{
    dummy_pid = fork();
    if (dummy_pid == 0)
    {
        execl("./dummy", "dummy", NULL);
    }
    else
    {
        FILE *fp = fopen("dummycpid.txt", "w");
        fprintf(fp, "%d\n", dummy_pid);
        fclose(fp);
    }
}

void printlineUnderneeth()
{
    for (int i = 1; i <= n; i++)
    {
        if (i == 1)
            printf("+");
        printf("--------");
        if (i == n)
            printf("\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <number_of_players>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    n = atoi(argv[1]);
    FILE *fp = fopen("childpid.txt", "w");
    if (!fp)
    {
        fprintf(stderr, "Failed to open childpid.txt\n");
        exit(1);
    }
    fprintf(fp, "%d\n", n);

    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);

    child_pids = (int *)malloc(n * sizeof(int));
    child_status = (int *)malloc(n * sizeof(int));
    memset(child_status, 0, n * sizeof(int));

    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(1);
        }
        else if (pid == 0)
        {
            char index_str[10];
            sprintf(index_str, "%d", i + 1);
            execl("./child", "child", index_str, NULL);
            perror("execl failed");
            exit(1);
        }
        else
        {
            child_pids[i] = pid;
            fprintf(fp, "%d\n", pid);
        }
    }
    fclose(fp);
    printf("Parent: %d child processes created\n", n);
    printf("Parent: Waiting for child processes to read child database\n");
    sleep(2);
    for (int i = 1; i <= n; i++)
    {
        printf("   %d    ", i);
        if (i == n)
            printf("\n");
    }
    printf("\n");
    int current_player = 0;
    bool gameOn = true;
    while (gameOn)
    {
        startDummycode();
        kill(child_pids[current_player], SIGUSR2);
        pause();
        if (signal_recieved == 1)
        {
            child_status[current_player] = 0;
        }
        else if (signal_recieved == 2)
        {
            child_status[current_player] = 1;
        }
        printlineUnderneeth();
        kill(child_pids[0], SIGUSR1);
        waitpid(dummy_pid, NULL, 0);
        if (count_playing_players(n, child_status) <= 1)
        {
            gameOn = false;
        }
        else
        {
            current_player = find_next_player(current_player, n, child_status);
        }
    }
    formatprint();
    // to end the all the children
    printf("\n");
    for (int i = 0; i < n; i++)
    {
        kill(child_pids[i], SIGINT);
    }
    free(child_pids);
    free(child_status);
    return 0;
}