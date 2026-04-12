#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
extern struct proc proc[NPROC];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

extern int disk_sched_policy; // 0 for FCFS, 1 for SSTF

uint64
sys_setdisksched(void)
{
  int policy;
  argint(0, &policy);
  if(policy != 0 && policy != 1)
    return -1;
  disk_sched_policy = policy;
  return 0;
}

extern int raid_policy;
extern int failed_disk;

uint64
sys_setraid(void)
{
  int policy;
  argint(0, &policy);
  if(policy != 0 && policy != 1 && policy != 5)
    return -1;
  raid_policy = policy;
  return 0;
}

uint64
sys_faildisk(void)
{
  int disk;
  argint(0, &disk);
  if (disk < -1 || disk > 3) return -1;
  failed_disk = disk;
  return 0;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_hello(void)
{
  printf("Hello from the kernel!\n");
  return 0;
}

uint64
sys_getpid2(void){
  return myproc()->pid;
}

uint64
sys_getppid(void)
{
  struct proc *parent=myproc()->parent;
  if(parent!=0){
    return parent->pid;
  }
  
  return -1;
}

uint64
sys_getnumchild(void){
  struct proc *cur=myproc();
  struct proc *p;
  int cnt=0;

  for(p=proc;p<&proc[NPROC];p++){
    acquire(&p->lock);
    if(p->parent==cur && p->state!=ZOMBIE && p->state!=UNUSED){
      cnt++;
    }
    release(&p->lock);
  }
  return cnt;
}

uint64
sys_getsyscount(void){
  struct proc *cur=myproc();
  return cur->syscount;
}

uint64
sys_getchildsyscount(void){
  int pid;
  argint(0,&pid);
  struct proc *cur=myproc();
  struct proc *p;
  for(p=proc;p<&proc[NPROC];p++){
    acquire(&p->lock);
    if(p->parent==cur && p->pid==pid){
      int count=p->syscount;
      release(&p->lock);
      return count;
    }
    release(&p->lock);
  }
  return -1;
}

uint64 sys_getlevel(void){
  return myproc()->mlfq_level;
}

uint64 sys_getmlfqinfo(void){
  int pid;
  argint(0,&pid);
  struct mlfqinfo info;
  uint64 adress;
  argaddr(1,&adress);

  struct proc *p;
  for(p=proc;p<&proc[NPROC];p++){
    acquire(&p->lock);
    if(p->pid==pid){
      info.level=p->mlfq_level;
      info.total_syscalls=p->syscount;
      for(int i=0;i<4;i++){
        info.ticks[i]=p->ticks_consumed_per_level[i];
      }
      info.times_scheduled=p->times_scheduled;
      int res=copyout(myproc()->pagetable,adress,(char *)&info,sizeof(info));
      release(&p->lock);
      if(res==-1){
        printf("Error occured during copying result from kernel space to user space\n");
        return -1;
      }
      return 0;
    }
    release(&p->lock);
  }
  printf("PID not found\n");
  return -1;
}

uint64 sys_getvmstats(void) {
    int pid;
    uint64 addr;
    argint(0,&pid);
    argaddr(1,&addr);

    struct vmstats info;
    struct proc *p;

    for (p=proc;p<&proc[NPROC];p++) {
        acquire(&p->lock);
        if(p->pid==pid){
            info.page_faults=p->page_faults;
            info.pages_evicted=p->pages_evicted;
            info.pages_swapped_in=p->pages_swapped_in;
            info.pages_swapped_out=p->pages_swapped_out;
            info.resident_pages=p->resident_pages;
            info.disk_reads = p->disk_reads;
            info.disk_writes = p->disk_writes;
            info.avg_disk_latency = p->avg_disk_latency;
            release(&p->lock);
            return copyout(myproc()->pagetable,addr,(char*)&info,sizeof(info));
        }
        release(&p->lock);
    }
    return -1;
}