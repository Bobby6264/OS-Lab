#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BLOCK_SIZE 3
char command;

// create a function to check if the block is valid
int is_valid_block(int block[BLOCK_SIZE][BLOCK_SIZE])
{
    int count[10] = {0};
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        for (int j = 0; j < BLOCK_SIZE; j++)
        {
            if (block[i][j] < 0 || block[i][j] > 9)
            {
                return 0;
            }
            count[block[i][j]]++;
        }
    }
    for (int i = 1; i <= 9; i++)
    {
        if (count[i] > 1)
        {
            return 0;
        }
    }
    return 1;
}

// Structure to store block state
typedef struct
{
    int initial[BLOCK_SIZE][BLOCK_SIZE]; // Original puzzle block
    int current_state[BLOCK_SIZE][BLOCK_SIZE];  // Current state
    int block_id;
    int pipe_input;
    int pipe_output;
    int row_neighbor1;
    int row_neighbor2;
    int col_neighbor1;
    int col_neighbor2;
} BlockState;

// Draw the block in the xterm window
void render_block(BlockState *state)
{
    // clear what ever there is in the terminal
    printf("\033[H\033[J");
    // printf("Block %d\n", state->block_id);
    printf("╔═══╤═══╤═══╗\n");

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        printf("║");
        for (int j = 0; j < BLOCK_SIZE; j++)
        {
            if (state->current_state[i][j] == 0)
                printf(" · ");
            else
                printf(" %d ", state->current_state[i][j]);
            if (j < BLOCK_SIZE - 1)
                printf("│");
        }
        printf("║\n");
        if (i < BLOCK_SIZE - 1)
            printf("╟───┼───┼───╢\n");
    }
    printf("╚═══╧═══╧═══╝\n");
    fflush(stdout);
}

// Check if placing digit d in cell (row, col) causes row conflict
int check_row_conflict(BlockState *state, int row, int digit)
{
    // Send check request to row neighbors
    int stdout_copy = dup(STDOUT_FILENO);
    if (stdout_copy == -1 || dup2(state->row_neighbor1, STDOUT_FILENO) == -1)
    {
        perror("dup or dup2 failed");
        exit(1);
    }
    printf("r %d %d %d\n", row, digit, state->pipe_output);
    if (dup2(state->row_neighbor2, STDOUT_FILENO) == -1)
    {
        perror("dup2 failed");
        exit(1);
    }
    printf("r %d %d %d\n", row, digit, state->pipe_output);
    if (dup2(stdout_copy, STDOUT_FILENO) == -1)
    {
        perror("dup2 restore failed");
        exit(1);
    }
    close(stdout_copy);

    int response1, response2;
    stdout_copy = dup(STDOUT_FILENO);
    if (stdout_copy == -1 || dup2(state->pipe_output, STDOUT_FILENO) == -1)
    {
        perror("dup or dup2 failed");
        exit(1);
    }

    scanf("%d", &response1);
    scanf("%d", &response2);

    if (dup2(stdout_copy, STDOUT_FILENO) == -1)
    {
        perror("dup2 restore failed");
        exit(1);
    }
    close(stdout_copy);

    return response1 || response2;
}

// Check if placing digit d in cell (row, col) causes column conflict
int check_column_conflict(BlockState *state, int col, int digit)
{
    // Send check request to column neighbors
    int stdout_copy = dup(STDOUT_FILENO);
    if (stdout_copy == -1 || dup2(state->col_neighbor1, STDOUT_FILENO) == -1)
    {
        perror("dup or dup2 failed");
        exit(1);
    }
    printf("c %d %d %d\n", col, digit, state->pipe_output);
    if (dup2(state->col_neighbor2, STDOUT_FILENO) == -1)
    {
        perror("dup2 failed");
        exit(1);
    }
    printf("c %d %d %d\n", col, digit, state->pipe_output);
    if (dup2(stdout_copy, STDOUT_FILENO) == -1)
    {
        perror("dup2 restore failed");
        exit(1);
    }
    int response1, response2;
    if (dup2(state->pipe_output, STDOUT_FILENO) == -1)
    {
        perror("dup or dup2 failed");
        exit(1);
    }

    scanf("%d", &response1);
    scanf("%d", &response2);

    if (dup2(stdout_copy, STDOUT_FILENO) == -1)
    {
        perror("dup2 restore failed");
        exit(1);
    }
    close(stdout_copy);

    return response1 || response2;
}

// Check if placing digit d in cell (row, col) causes block conflict
int check_block_conflict(BlockState *state, int row, int col, int digit)
{
    // Check for block conflict
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        for (int j = 0; j < BLOCK_SIZE; j++)
        {
            if (state->current_state[i][j] == digit)
            {
                return 1;
            }
        }
    }
    return 0;
}

// Handle place command
void handle_place(BlockState *state, int cell, int digit)
{
    int row = cell / 3;
    int col = cell % 3;

    // Check if cell is in original puzzle
    if (state->initial[row][col] != 0)
    {
        printf("Error: Read-only cell\n");
        sleep(2);
        render_block(state);
        return;
    }

    // Check for block conflict
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        for (int j = 0; j < BLOCK_SIZE; j++)
        {
            if (state->current_state[i][j] == digit)
            {
                printf("Error: Block conflict\n");
                sleep(2);
                printf("\033[H\033[J");
                render_block(state);
                return;
            }
        }
    }

    // Check for row and column conflicts
    if (check_row_conflict(state, row, digit))
    {
        printf("Error: Row conflict\n");
        sleep(2);
        printf("\033[H\033[J");
        render_block(state);
        return;
    }

    if (check_column_conflict(state, col, digit))
    {
        printf("Error: Column conflict\n");
        sleep(2);
        render_block(state);
        return;
    }

    // If all checks pass, place the digit
    state->current_state[row][col] = digit;
    render_block(state);
}

// Handle row check request
void handle_row_check(BlockState *state, int row, int digit, int response_fd)
{
    int conflict = 0;
    for (int j = 0; j < BLOCK_SIZE; j++)
    {
        if (state->current_state[row][j] == digit)
        {
            conflict = 1;
            break;
        }
    }

    int stdout_copy = dup(STDOUT_FILENO);
    if (stdout_copy == -1 || dup2(response_fd, STDOUT_FILENO) == -1)
    {
        perror("dup or dup2 failed");
        exit(1);
    }

    printf("%d\n", conflict);

    if (dup2(stdout_copy, STDOUT_FILENO) == -1)
    {
        perror("dup2 restore failed");
        exit(1);
    }
    close(stdout_copy);
}

// create a function to to clear the terminal
void clear_terminal()
{
    printf("\033[H\033[J");
}


// Handle column check request
void handle_column_check(BlockState *state, int col, int digit, int response_fd)
{
    int conflict = 0;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (state->current_state[i][col] == digit)
        {
            conflict = 1;
            break;
        }
    }

    int stdout_copy = dup(STDOUT_FILENO);
    if (stdout_copy == -1 || dup2(response_fd, STDOUT_FILENO) == -1)
    {
        perror("dup or dup2 failed");
        exit(1);
    }

    printf("%d\n", conflict);

    if (dup2(stdout_copy, STDOUT_FILENO) == -1)
    {
        perror("dup2 restore failed");
        exit(1);
    }
    close(stdout_copy);
}



int main(int argc, char *argv[])
{
    if (argc != 8)
    {
        fprintf(stderr, "Usage: %s block_id pipe_input pipe_output row_neighbor1 row_neighbor2 col_neighbor1 col_neighbor2\n", argv[0]);
        exit(1);
    }

    BlockState state;
    state.block_id = atoi(argv[1]);
    state.pipe_input = atoi(argv[2]);
    state.pipe_output = atoi(argv[3]);
    state.row_neighbor1 = atoi(argv[4]);
    state.row_neighbor2 = atoi(argv[5]);
    state.col_neighbor1 = atoi(argv[6]);
    state.col_neighbor2 = atoi(argv[7]);

    // Redirect stdin to pipe_input
    dup2(state.pipe_input, STDIN_FILENO);
    close(state.pipe_input);


    while (scanf(" %c", &command) != EOF)
    {
        switch (command)
        {
        case 'n':
        {
            // Read block values
            for (int i = 0; i < BLOCK_SIZE; i++)
            {
                for (int j = 0; j < BLOCK_SIZE; j++)
                {
                    if (scanf("%d", &state.initial[i][j]) != 1)
                    {
                        fprintf(stderr, "Error: Invalid input for block values\n");
                        exit(1);
                    }
                    state.current_state[i][j] = state.initial[i][j];
                }
            }
            render_block(&state);
            break;
        }

        case 'p':
        {
            int cell, digit;
            if (scanf("%d %d", &cell, &digit) != 2)
            {
                fprintf(stderr, "Error: Invalid input for place command\n");
                exit(1);
            }
            if (cell < 0 || cell >= BLOCK_SIZE * BLOCK_SIZE || digit < 1 || digit > 9)
            {
                fprintf(stderr, "Error: Invalid cell or digit value\n");
                exit(1);
            }
            handle_place(&state, cell, digit);
            break;
        }

        case 'r':
        {
            int row, digit, response_fd;
            if (scanf("%d %d %d", &row, &digit, &response_fd) != 3)
            {
                fprintf(stderr, "Error: Invalid input for row check command\n");
                exit(1);
            }
            if (row < 0 || row >= BLOCK_SIZE || digit < 1 || digit > 9)
            {
                fprintf(stderr, "Error: Invalid row or digit value\n");
                exit(1);
            }
            handle_row_check(&state, row, digit, response_fd);
            break;
        }

        case 'c':
        {
            int col, digit, response_fd;
            if (scanf("%d %d %d", &col, &digit, &response_fd) != 3)
            {
                fprintf(stderr, "Error: Invalid input for column check command\n");
                exit(1);
            }
            if (col < 0 || col >= BLOCK_SIZE || digit < 1 || digit > 9)
            {
                fprintf(stderr, "Error: Invalid column or digit value\n");
                exit(1);
            }
            handle_column_check(&state, col, digit, response_fd);
            break;
        }

        case 'q':
        {
            printf("Bye from Block %d\n", state.block_id);
            fflush(stdout);
            sleep(2);
            exit(0);
        }

        default:
        {
            fprintf(stderr, "Error: Unknown command '%c'\n", command);
            exit(1);
        }
        }
    }

    return 0;
}
