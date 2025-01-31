#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "boardgen.c"

#define BLOCK_COUNT 9
#define BLOCK_SIZE 3
#define BOARD_SIZE 9
char input[32];
bool isvalid;
size_t input_len;

// Structure to hold pipe file descriptors for each block
typedef struct
{
    int read_fd;
    int write_fd;
} PipePair;

bool checker(size_t input_len, int limit)
{
    if (input_len > limit)
    {
        int valid = 1;
        for (size_t i = 1; i < input_len; i++)
        {
            if (input[i] != ' ')
            {
                valid = 0;
                break;
                return false;
            }
        }
        if (!valid)
        {
            return false;
        }
    }
    return true;
}

// Function to get row neighbors for a given block
void get_row_neighbors(int block, int *neighbor1, int *neighbor2)
{
    int row = block / 3;
    int base = row * 3;
    *neighbor1 = base + (block + 1) % 3;
    *neighbor2 = base + (block + 2) % 3;
}

// Function to get column neighbors for a given block
void get_column_neighbors(int block, int *neighbor1, int *neighbor2)
{
    *neighbor1 = (block + 3) % 9;
    if (*neighbor1 / 3 == block / 3)
    {
        *neighbor1 = (block + 6) % 9;
    }
    *neighbor2 = (block + 6) % 9;
    if (*neighbor2 / 3 == block / 3)
    {
        *neighbor2 = (block + 3) % 9;
    }
}

// Launch xterm for a block with appropriate geometry
void launch_block(int block_num, PipePair *pipes, int rn1, int rn2, int cn1, int cn2)
{
    char geometry[32];
    int x = (block_num % 3) * 223 + 1200; // Adjust x to place on the right half of the screen
    int y = (block_num / 3) * 233 + 150;
    snprintf(geometry, sizeof(geometry), "17x8+%d+%d", x, y);

    char title[32];
    snprintf(title, sizeof(title), "Block %d", block_num);

    char block_num_str[8], pipe_in[8], pipe_out[8];
    char rn1_out[8], rn2_out[8], cn1_out[8], cn2_out[8];

    snprintf(block_num_str, sizeof(block_num_str), "%d", block_num);
    snprintf(pipe_in, sizeof(pipe_in), "%d", pipes[block_num].read_fd);
    snprintf(pipe_out, sizeof(pipe_out), "%d", pipes[block_num].write_fd);
    snprintf(rn1_out, sizeof(rn1_out), "%d", pipes[rn1].write_fd);
    snprintf(rn2_out, sizeof(rn2_out), "%d", pipes[rn2].write_fd);
    snprintf(cn1_out, sizeof(cn1_out), "%d", pipes[cn1].write_fd);
    snprintf(cn2_out, sizeof(cn2_out), "%d", pipes[cn2].write_fd);

    execlp("xterm", "xterm",
           "-T", title,
           "-fa", "Monospace",
           "-fs", "15",
           "-geometry", geometry,
           "-bg", "white",
           "-e", "./block",
           block_num_str, pipe_in, pipe_out,
           rn1_out, rn2_out, cn1_out, cn2_out,
           NULL);

    perror("execlp failed");
    exit(1);
}

int main()
{
    PipePair pipes[BLOCK_COUNT];
    pid_t children[BLOCK_COUNT];
    int A[BOARD_SIZE][BOARD_SIZE], S[BOARD_SIZE][BOARD_SIZE];
    // char command;

    // Create pipes
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        int fd[2];
        if (pipe(fd) == -1)
        {
            perror("pipe creation failed");
            exit(1);
        }
        pipes[i].read_fd = fd[0];
        pipes[i].write_fd = fd[1];
    }

    // Fork children
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        children[i] = fork();
        if (children[i] == -1)
        {
            perror("fork failed");
            exit(1);
        }

        if (children[i] == 0)
        { // Child process
            // Close unused pipe ends
            for (int j = 0; j < BLOCK_COUNT; j++)
            {
                if (j != i)
                    close(pipes[j].read_fd);
            }

            int rn1, rn2, cn1, cn2;
            get_row_neighbors(i, &rn1, &rn2);
            get_column_neighbors(i, &cn1, &cn2);

            launch_block(i, pipes, rn1, rn2, cn1, cn2);
            exit(0); // Should not reach here
        }
    }

    // Parent process
    printf("\n\n");
    printf("     +----------------------------------------------------------------------------------+\n");
    printf("     |                                 INSTRUCTIONS                                     |\n");
    printf("     +----------------------------------------------------------------------------------+\n");
    printf("     |    1  .  Foodoku Game Started                                                    |\n");
    printf("     |    2  .  Commands:                                                               |\n");
    printf("     |                h - Help                                                          |\n");
    printf("     |                n - New game                                                      |\n");
    printf("     |                p b c d - Place digit d in cell c of block b                      |\n");
    printf("     |                s - Show solution                                                 |\n");
    printf("     |                q - Quit                                                          |\n");
    printf("     |    3  .  REMEMBER  :  DO NOT PUT EXTRA SPACES ANY WHERE                          |\n");
    printf("     +----------------------------------------------------------------------------------+\n\n");

    while (1)
    {
        printf("\nEnter command: ");
        fgets(input, sizeof(input), stdin);

        switch (input[0])
        {
        case 'h':

            input_len = strlen(input);

           isvalid = checker(input_len, 2);
            if (!isvalid)
            {
                printf("Invalid input\n");
                continue;
            }

            printf("\nHelp:\n");
            printf("Blocks are numbered 0-8 from left to right, top to bottom\n");
            printf("Cells in each block are numbered 0-8 similarly\n");
            printf("Valid digits are 1-9\n");
            break;

        case 'n':
        {

            isvalid = checker(input_len, 2);
            if (!isvalid)
            {
                printf("Invalid input\n");
                continue;
            }

            newboard(A, S); // Generate new puzzle

            // Send blocks to respective processes
            for (int i = 0; i < BLOCK_COUNT; i++)
            {
                int stdout_copy = dup(STDOUT_FILENO);
                if (stdout_copy == -1 || dup2(pipes[i].write_fd, STDOUT_FILENO) == -1)
                {
                    perror("dup or dup2 failed");
                    exit(1);
                }

                printf("n ");
                int row_start = (i / 3) * 3;
                int col_start = (i % 3) * 3;

                for (int r = 0; r < 3; r++)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        printf("%d ", A[row_start + r][col_start + c]);
                    }
                }
                printf("\n");

                if (dup2(stdout_copy, STDOUT_FILENO) == -1)
                {
                    perror("dup2 restore failed");
                    exit(1);
                }
                close(stdout_copy);
            }
            break;
        }

        case 'p':
        {
            isvalid = checker(strlen(input), 8);
            if (!isvalid)
            {
                printf("Invalid input\n");
                continue;
            }
            int b, c, d;
            if (sscanf(input, "p %d %d %d", &b, &c, &d) != 3)
            {
                printf("Invalid input format\n");
                continue;
            }

            if (b < 0 || b >= 9 || c < 0 || c >= 9 || d < 1 || d > 9)
            {
                printf("Invalid input ranges\n");
                continue;
            }

            int stdout_copy = dup(STDOUT_FILENO);
            if (stdout_copy == -1 || dup2(pipes[b].write_fd, STDOUT_FILENO) == -1)
            {
                perror("dup or dup2 failed");
                exit(1);
            }

            printf("p %d %d\n", c, d);

            if (dup2(stdout_copy, STDOUT_FILENO) == -1)
            {
                perror("dup2 restore failed");
                exit(1);
            }
            close(stdout_copy);
            break;
        }

        case 's':
        {

            isvalid = checker(input_len, 2);
            if (!isvalid)
            {
                printf("Invalid input\n");
                continue;
            }
            // Send solution blocks to processes
            for (int i = 0; i < BLOCK_COUNT; i++)
            {
                int stdout_copy = dup(STDOUT_FILENO);
                if (stdout_copy == -1 || dup2(pipes[i].write_fd, STDOUT_FILENO) == -1)
                {
                    perror("dup or dup2 failed");
                    exit(1);
                }

                printf("n ");
                int row_start = (i / 3) * 3;
                int col_start = (i % 3) * 3;

                for (int r = 0; r < 3; r++)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        printf("%d ", S[row_start + r][col_start + c]);
                    }
                }
                printf("\n");

                if (dup2(stdout_copy, STDOUT_FILENO) == -1)
                {
                    perror("dup2 restore failed");
                    exit(1);
                }
                close(stdout_copy);
            }
            break;
        }

        case 'q':

            isvalid = checker(input_len, 2);
            if (!isvalid)
            {
                printf("Invalid input\n");
                continue;
            }
            // Send quit command to all blocks
            for (int i = 0; i < BLOCK_COUNT; i++)
            {
                int stdout_copy = dup(STDOUT_FILENO);
                if (stdout_copy == -1 || dup2(pipes[i].write_fd, STDOUT_FILENO) == -1)
                {
                    perror("dup or dup2 failed");
                    exit(1);
                }

                printf("q\n");

                if (dup2(stdout_copy, STDOUT_FILENO) == -1)
                {
                    perror("dup2 restore failed");
                    exit(1);
                }
                close(stdout_copy);
            }

            // Wait for all children to exit
            for (int i = 0; i < BLOCK_COUNT; i++)
            {
                wait(NULL);
            }

            printf("Game ended\n");
            exit(0);

        default:
            printf("Invalid command\n");
        }
    }

    return 0;
}
