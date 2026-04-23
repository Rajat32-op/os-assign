#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if(argc != 2){
        printf("Usage: setraid <0|1|5>\n");
        exit(1);
    }
    
    int level = atoi(argv[1]);
    if(level != 0 && level != 1 && level != 5){
         printf("Invalid RAID level. Must be 0, 1, or 5.\n");
         exit(1);
    }
    
    if(setraid(level) < 0){
        printf("Failed to set RAID to %d\n", level);
        exit(1);
    }
    
    printf("RAID level successfully set to %d\n", level);
    exit(0);
}
