#include "kernel/types.h"
#include "user/user.h"

int main(){
    int pid=getppid();
    printf("PID of Parent of current process: %d\n",pid);
    exit(0);
}