#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "swap.h"

struct swap_slot swap_table[NSWAP];
char swap_data[NSWAP][PGSIZE];
struct spinlock  swap_lock;

void swapinit(void) {
    initlock(&swap_lock,"swap");
    for (int i=0; i<NSWAP;i++) {
        swap_table[i].in_use=0;
        swap_table[i].pid=-1;
        swap_table[i].va=0;
    }
}

int swap_alloc(int pid, uint64 va) {
    acquire(&swap_lock);
    for (int i=0;i<NSWAP;i++) {
        if (!swap_table[i].in_use) {
            swap_table[i].in_use =1;
            swap_table[i].pid=pid;
            swap_table[i].va=va;
            release(&swap_lock);
            return i;
        }
    }
    release(&swap_lock);
    return -1;
}

void swap_free(int slot) {
    acquire(&swap_lock);
    swap_table[slot].in_use=0;
    swap_table[slot].pid=-1;
    swap_table[slot].va=0;
    release(&swap_lock);
}

void swap_write(int slot,char *src) {
    memmove(swap_data[slot],src,PGSIZE);
}

void swap_read(int slot, char *dst) {
    memmove(dst,swap_data[slot],PGSIZE);
}