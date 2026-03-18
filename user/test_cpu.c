#include "kernel/types.h"
#include "user/user.h"

int main(void)
{
  int child = fork();
  if(child == 0){
    long x = 1;
    while(1){
      for(long i = 0; i < 1000L; i++)
        x = x * 3 + i;
    }
  }

  printf("CPU-bound test: monitoring child pid=%d\n", child);
  for(int i = 0; i < 5; i++){
    pause(10);
    struct mlfqinfo info;
    if(getmlfqinfo(child, &info) == 0){
      printf("t=%d level=%d  ticks=[%d,%d,%d,%d]  scheduled=%d\n",
        (i+1)*10, info.level,
        info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3],
        info.times_scheduled);
    }
  }
  kill(child);
  wait(0);
  exit(0);
}
