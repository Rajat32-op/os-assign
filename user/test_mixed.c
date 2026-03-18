#include "kernel/types.h"
#include "user/user.h"

int main(void)
{
  int pid_cpu = fork();
  if(pid_cpu == 0){
    long x = 1;
    while(1){
      for(long i = 0; i < 10000; i++)
        x = x * 3 + i;
    }
  }

  int pid_sys = fork();
  if(pid_sys == 0){
    while(1){
      for(int i = 0; i < 10000; i++)
        getpid();
    }
  }

  printf("Mixed test: cpu_pid=%d  sys_pid=%d\n", pid_cpu, pid_sys);

  for(int i = 0; i < 12; i++){
    pause(10);
    struct mlfqinfo c, s;
    getmlfqinfo(pid_cpu, &c);
    getmlfqinfo(pid_sys, &s);
    printf("t=%d  CPU: lv=%d ticks=[%d,%d,%d,%d] scheduled=%d  SYS: lv=%d ticks=[%d,%d,%d,%d] scheduled=%d\n",
      (i+1)*10,
      c.level, c.ticks[0], c.ticks[1], c.ticks[2], c.ticks[3], c.times_scheduled,
      s.level, s.ticks[0], s.ticks[1], s.ticks[2], s.ticks[3], s.times_scheduled);
  }

  kill(pid_cpu);
  kill(pid_sys);
  wait(0);
  wait(0);
  printf("Mixed test done\n");
  exit(0);
}
