#include <bits/stdc++.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
using namespace std;

// global variables
int F = 1;

void follower_process(int follower_id, int *shm) {
    srand(time(NULL) ^ (follower_id * 1000));
    
    while (1) {
        while (shm[2] != follower_id && shm[2] != -follower_id) {
            usleep(1000);
        }

        if (shm[2] == -follower_id) {
            if (follower_id == shm[0]) {
                shm[2] = 0;
            } else {
                shm[2] = -(follower_id + 1);
            }
            printf("follower %d leaves\n", follower_id);
            return;
        }

        shm[4 + follower_id] = rand() % 9 + 1;

        if (follower_id == shm[0]) {
            shm[2] = 0;
        } else {
            shm[2] = follower_id + 1;
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc > 1)
    {
        int num = atoi(argv[1]);
        if (num <= 0)
        {
            cout << "Invalid number of followers. The system continues with the default value 1" << endl;
        }
        else
        {
            F = num;
        }
    }

    key_t key = ftok("/", 'A');
    int shmsize = 10 + 4; // totalF + 4, assuming totalF is 10 as default
    int shmid = shmget(key, shmsize * sizeof(int), 0777); // giving permision 0777
    int *M = (int *)shmat(shmid, NULL, 0);

    for (int i = 0; i < F; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            int my_ID = M[1]+1;
            if(my_ID > M[0]){
                cout << "Follower " << my_ID << " is not allowed to join as the limit is reached" << endl;
                exit(0);
            }
            M[1] = my_ID;

            cout << "Follower " << my_ID << " joined" << endl;
            follower_process(my_ID, M);
            shmdt(M);
            exit(0);
        }
    }
    for (int i = 0; i < F; i++) {
        wait(NULL);
    }

    shmdt(M);
    return 0;
}