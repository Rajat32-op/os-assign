#include "kernel/types.h"
#include "user/user.h"

int main(void)
{
  int child = fork();
  if(child == 0){
    while(1){
      for(int i = 0; i < 10000; i++)
        getpid();
    }
  }

  printf("Syscall-heavy test: monitoring child pid=%d\n", child);
  for(int i = 0; i < 5; i++){
    pause(10);
    struct mlfqinfo info;
    if(getmlfqinfo(child, &info) == 0){
      printf("t=%d  level=%d  scheduled=%d  syscalls=%d\n",
        (i+1)*10, info.level,
        info.times_scheduled, info.total_syscalls);
    }
  }
  kill(child);
  wait(0);
  exit(0);
}
