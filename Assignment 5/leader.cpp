#include <bits/stdc++.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

using namespace std;

// global variables
int totalF = 10;
vector<int> hashTable(1000, -1);

int main(int argc, char *argv[])
{
    // if there is an argument then took input
    if (argc > 1)
    {
        totalF = atoi(argv[1]);
    }
    if (totalF <= 0 || totalF > 100)
    {
        cout << "\nOut of range the value of totalFollowers. The system continues with the deafault value 10\n"
             << endl;
        totalF = 10;
    }

    key_t key = ftok("/", 'A');
    int shmsize = totalF + 4; // for m[0],m[1],m[2],m[3]
    int shmid = shmget(key, shmsize * sizeof(int), IPC_CREAT | 0777); // giving permision 0777
    int *M = (int *)shmat(shmid, NULL, 0);

    // declaring M[0],M[1],M[2],M[3]
    M[0] = totalF;
    M[1] = 0;
    M[2] = 0;

    // wait until all followers joined
    cout << "Waiting for all followers to join..." << endl;
    while (M[1] < totalF)
    {
        sleep(1);
    }

    while (true)
    {
        // this is the first time when it starts for the process
        while (M[2] != 0)
        {
            usleep(100); // this had been use to avoid the race condition
        }

        M[2] = 1;
        int randomNumber = rand() % 99 + 1;
        M[3] = randomNumber;

        // this is the second time it wake ups from the sleep , means
        while (M[2] != 0)
        {
            usleep(100); // to avoid the race condition
        }

        cout << M[3];
        int sum = M[3];
        for (int i = 1; i <= totalF; i++)
        {
            sum += M[4 + i];
            cout << " +" << M[4 + i];
        }

        cout << " = " << sum << endl;

        if (hashTable[sum] != -1)
        {
            M[2] = -1;
            // this will wake up for the last time when it needs to break
            while (M[2] != 0)
            {
                usleep(100);
            }
            break;
        }else{
            hashTable[sum] = 1;
        }
    }

    // free up the shared memory
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}