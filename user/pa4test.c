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

void test_raid_integrity(int raid_level, int disk_to_fail) {
    printf("\n--- Running Test for RAID %d ---\n", raid_level);
    
    struct vmstats s_before,s_after;
    get_stats(&s_before);
    
    int n=NFRAMES+50;
    char *b=sbrklazy(n*PGSIZE);
    
    // Write data to force swap
    for(int i=0;i<n;i++){
        b[i*PGSIZE]=(char)(i&0xff);
    }
    
    if (disk_to_fail >= 0) {
        printf("  [INFO] Failing disk %d to test RAID reconstruction\n", disk_to_fail);
        faildisk(disk_to_fail);
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
           
    check("data intact after eviction/reconstruction",errs==0);
    check("disk reads > 0",reads>0);
    check("disk writes > 0",writes>0);
    
    if (disk_to_fail >= 0) {
        faildisk(-1); 
    }
    
    sbrk(-(n*PGSIZE));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: pa4test <raid_level_tested>\n");
        printf("Before running, ensure correct RAID level using: setraid <0|1|5>\n");
        exit(1);
    }
    
    int raid_level = atoi(argv[1]);
    
    printf("=== PA4 Test for RAID %d ===\n", raid_level);
    
    if (raid_level == 0) {
        test_raid_integrity(0, -1);
    } else if (raid_level == 1) {
        test_raid_integrity(1, 0); // Fail disk 0 for RAID 1
    } else if (raid_level == 5) {
        test_raid_integrity(5, 2); // Fail disk 2 (parity/data) for RAID 5
    } else {
        printf("Unknown RAID level to test. Please specify 0, 1, or 5.\n");
        exit(1);
    }

    printf("\n=== Results: %d passed, %d failed ===\n",pass,fail);
    exit(0);
}
