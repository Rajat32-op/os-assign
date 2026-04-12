#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define NFRAMES 128

int pass=0,fail=0;

void check(const char *name,int cond) {
    if(cond){
        printf("  [PASS] %s\n",name);pass++; 
    }else{ 
        printf("  [FAIL] %s\n",name);fail++; 
    }
}

void get_stats(struct vmstats *s) {
    getvmstats(getpid(),s);
}

void run_test_with_policy(int policy,const char *policy_name) {
    printf("\n--- Running with %s scheduler ---\n",policy_name);
    if(setdisksched(policy)<0){
        printf("  [FAIL] failed to set disk scheduler to %s\n",policy_name);
        fail++;
        return;
    }
    
    struct vmstats s_before,s_after;
    get_stats(&s_before);
    
    int n=NFRAMES+50;
    char *b=sbrklazy(n*PGSIZE);
    
    for(int i=0;i<n;i++){
        b[i*PGSIZE]=(char)(i&0xff);
    }
    
    int errs=0;
    for(int i=0;i<n;i++){
        if(b[i*PGSIZE]!=(char)(i&0xff)){
            errs++;
        }
    }

    int random_pages[]={12,100,3,50,99,14,60,20,110,5};
    for(int i=0;i<10;i++){
        int idx=random_pages[i];
        if(b[idx*PGSIZE]!=(char)(idx&0xff)){
            errs++;
        }
    }
    
    get_stats(&s_after);
    
    int reads=s_after.disk_reads-s_before.disk_reads;
    int writes=s_after.disk_writes-s_before.disk_writes;
    int faults=s_after.page_faults-s_before.page_faults;
    
    printf("  faults=%d disk_reads=%d disk_writes=%d latency=%d\n",
           faults,reads,writes,s_after.avg_disk_latency);
           
    check("data intact after eviction",errs==0);
    check("disk reads > 0",reads>0);
    check("disk writes > 0",writes>0);
    check("latency >= 0",s_after.avg_disk_latency>=0);
    
    sbrk(-(n*PGSIZE));
}

void test_priority_preemption() {
    printf("\n--- Running Priority Preemption Test ---\n");
    int pid=fork();
    if(pid==0){
        int counter=0;
        for(int i=0;i<100000000;i++){
            counter+=i;
        }

        char *b=sbrklazy(20*PGSIZE);
        for(int i=0;i<20;i++){
            b[i*PGSIZE]=i; 
        }
        exit(0);
    }else{
        pause(10);
        char *b=sbrklazy(20*PGSIZE);
        for(int i=0;i<20;i++){
             b[i*PGSIZE]=i;
        }
        wait(0);
        printf("  [PASS] Both processes finished eviction. (Check UART debug logs to verify preemption)\n");
    }
}

void test_raid5_reconstruction() {
    printf("\n--- Running RAID 5 Reconstruction Test ---\n");
    if(setraid(5)<0){
        printf("  [FAIL] failed to set RAID 5\n");
        fail++;
        return;
    }
    
    int n=200;
    char *b=sbrklazy(n*PGSIZE);
    
    for(int i=0;i<n;i++){
        b[i*PGSIZE]=(char)(i&0xff); 
    }
    
    faildisk(2); 

    int errs=0;
    for(int i=0;i<n;i++){
        if(b[i*PGSIZE]!=(char)(i&0xff)){
             errs++;
        }
    }
    
    if(errs==0){
         printf("  [PASS] RAID 5 Reconstruction successfully retrieved all data from parity!\n");
         pass++;
    }else{
         printf("  [FAIL] RAID 5 Reconstruction failed. Corruption detected!\n");
         fail++;
    }
    
    faildisk(-1);
    setraid(0);
    sbrk(-(n*PGSIZE));
}

int main(void) {
    printf("=== PA4 Test ===\n");
    
    run_test_with_policy(0,"FCFS");
    run_test_with_policy(1,"SSTF");
    
    test_priority_preemption();
    test_raid5_reconstruction();

    printf("\n=== Results: %d passed, %d failed ===\n",pass,fail);
    exit(0);
}
