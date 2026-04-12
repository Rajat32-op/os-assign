// Credit: Akshat
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    printf("\n--- Test 3: Scheduler-Aware Eviction ---\n");

    int pid_A = fork();
    if (pid_A == 0) {
        
        volatile long x = 0;
        for(long i = 0; i < 400000000L; i++) x += i; 
        
        // Allocate a baseline working set
        int pages = 80;
        char *mem = sbrklazy(pages * 4096);
        for(int i = 0; i < pages; i++) mem[i * 4096] = 'A';
        
        // Sleep so access bits age/clear, placing it on equal footing with B
        pause(50);
        exit(0);
    }

    int pid_B = fork();
    if (pid_B == 0) {
        // CHILD B: IO Bound
        // Light syscall work keeps priority high (Level 0/1)
        for(int i=0; i<3000; i++) getpid();
        
        // Allocate an identical working set
        int pages = 180;
        char *mem = sbrklazy(pages * 4096);
        for(int i = 0; i < pages; i++) mem[i * 4096] = 'B';
        
        // Sleep so access bits age/clear, placing it on equal footing with A
        pause(50);
        exit(0);
    }

    pause(15);
    
 
    int parent_pages = 60; 
    char *pb = sbrklazy(parent_pages * 4096);
    for (int i = 0; i < parent_pages; i++) pb[i * 4096] = 'P';
    
    pause(10); // Wait for clock evicter to stabilize

    struct vmstats st_A, st_B;
    getvmstats(pid_A, &st_A);
    getvmstats(pid_B, &st_B);
    
    printf("Child A (CPU Bound) Evictions: %d\n", st_A.pages_evicted);
    printf("Child B (IO Bound) Evictions: %d\n", st_B.pages_evicted);
    
    kill(pid_A);
    kill(pid_B);
    wait(0);
    wait(0);
    
    printf("Test 3 Complete. (Child A should have more evictions than Child B)\n\n");
    exit(0);
}
