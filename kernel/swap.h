#ifndef SWAP_H
#define SWAP_H

#include "types.h"
#include "spinlock.h"
#include "param.h"
#include "riscv.h"

struct swap_slot {
    int   in_use;
    int   pid;
    uint64 va;      
}; 

extern struct swap_slot swap_table[NSWAP];
extern char swap_data[NSWAP][PGSIZE];
extern struct spinlock  swap_lock;

void  swapinit(void);
int   swap_alloc(int pid, uint64 va);         
void  swap_free(int slot);
void  swap_write(int slot, char *src);         
void  swap_read(int slot, char *dst);          

#endif