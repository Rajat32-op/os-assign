#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define NFRAMES 128

void test_bounds() {
    printf("\n--- Profiling Evictions: CPU bound vs I/O bound ---\n");

    int pid_A = fork();
    if (pid_A == 0) {
        volatile long x = 0;
        for(long i = 0; i < 400000000L; i++) x += i; 
        
        int pages = 80;
        char *mem = sbrklazy(pages * PGSIZE);
        for(int i = 0; i < pages; i++) mem[i * PGSIZE] = 'A';
        
        pause(50);
        exit(0);
    }

    int pid_B = fork();
    if (pid_B == 0) {
        for(int i=0; i<3000; i++) getpid();
        
        int pages = 80;
        char *mem = sbrklazy(pages * PGSIZE);
        for(int i = 0; i < pages; i++) mem[i * PGSIZE] = 'B';
        
        pause(50);
        exit(0);
    }

    pause(15);
    
    int parent_pages = 60; 
    char *pb = sbrklazy(parent_pages * PGSIZE);
    for (int i = 0; i < parent_pages; i++) pb[i * PGSIZE] = 'P';
    
    pause(10); 

    struct vmstats st_A, st_B;
    getvmstats(pid_A, &st_A);
    getvmstats(pid_B, &st_B);
    
    printf("  [CPU BOUND] pid=%d evictions=%d faults=%d\n", pid_A, st_A.pages_evicted, st_A.page_faults);
    printf("  [I/O BOUND] pid=%d evictions=%d faults=%d\n", pid_B, st_B.pages_evicted, st_B.page_faults);
    
    if (st_A.pages_evicted > st_B.pages_evicted) {
         printf("  [PASS] CPU bound evictions (%d) > IO bound evictions (%d)\n", st_A.pages_evicted, st_B.pages_evicted);
    } else {
         printf("  [FAIL] Expected CPU evictions > IO evictions\n");
    }
    
    kill(pid_A);
    kill(pid_B);
    wait(0);
    wait(0);
    sbrk(-(parent_pages * PGSIZE));
}

int main(void) {
    printf("=== Starting RAID 5 Test ===\n");
    if(setraid(5) < 0) {
        printf("  [FAIL] Failed to set RAID 5\n");
        exit(1);
    }
    
    struct vmstats s_before, s_after;
    getvmstats(getpid(), &s_before);
    
    int n = NFRAMES + 50;
    char *b = sbrklazy(n * PGSIZE);
    
    for(int i = 0; i < n; i++) {
        b[i * PGSIZE] = (char)(i & 0xff);
    }
    
    printf("  [INFO] Failing disk 2 to test RAID 5 reconstruction\n");
    faildisk(2);
    
    int errs = 0;
    for(int i = 0; i < n; i++) {
        if(b[i * PGSIZE] != (char)(i & 0xff)) {
            errs++;
        }
    }
    
    getvmstats(getpid(), &s_after);
    
    int swapped_out = s_after.pages_swapped_out - s_before.pages_swapped_out;
    int swapped_in = s_after.pages_swapped_in - s_before.pages_swapped_in;
    int evicted = s_after.pages_evicted - s_before.pages_evicted;
    
    printf("\n--- Single Process Validation ---\n");
    printf("  pages_evicted = %d\n  pages_swapped_out = %d\n  pages_swapped_in = %d\n", evicted, swapped_out, swapped_in);
           
    if (errs == 0) {
        printf("  [PASS] Data intact after eviction and reconstruction\n");
    } else {
        printf("  [FAIL] Data corrupted! Errors: %d\n", errs);
    }
    
    if(evicted > 0) printf("  [PASS] pages evicted: %d\n", evicted); else printf("  [FAIL] pages evicted <= 0\n");
    if(swapped_out > 0) printf("  [PASS] pages swapped out: %d\n", swapped_out); else printf("  [FAIL] pages swapped out <= 0\n");
    if(swapped_in > 0) printf("  [PASS] pages swapped in: %d\n", swapped_in); else printf("  [FAIL] pages swapped in <= 0\n");
    
    faildisk(-1); // Recover disk
    sbrk(-(n * PGSIZE));
    
    test_bounds();
    
    printf("=== RAID 5 Test Finished ===\n");
    exit(0);
}
