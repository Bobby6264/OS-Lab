#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<stdlib.h>

int main(){
    while(1){
        wait(NULL);
    }
    return 0;
}