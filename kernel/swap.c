#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "swap.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct swap_slot swap_table[NSWAP];
// char swap_data[NSWAP][PGSIZE]; // Removed in-memory swap
struct spinlock  swap_lock;

// 0: RAID 0, 1: RAID 1, 5: RAID 5
int raid_policy = 0;
int failed_disk = -1; // -1 means no disk failed

#define NDISKS 4
#define BLKS_PER_DISK 2048 // Sufficient blocks for NSWAP * 4
#define SWAP_START 2000    // Past FSSIZE

// Simulate disk mapped
uint physical_block(int disk, int blk) {
  return SWAP_START + disk * BLKS_PER_DISK + blk;
}

void raid_write(uint logical_b, char *src) {
  if (raid_policy == 0) {
    int disk = logical_b % NDISKS;
    int blk = logical_b / NDISKS;
    struct buf *b = bread(1, physical_block(disk, blk));
    memmove(b->data, src, BSIZE);
    bwrite(b);
    brelse(b);
  } else if (raid_policy == 1) {
    int blk = logical_b / (NDISKS / 2);
    int disk1 = logical_b % (NDISKS / 2);
    int disk2 = disk1 + (NDISKS / 2);
    
    struct buf *b1 = bread(1, physical_block(disk1, blk));
    memmove(b1->data, src, BSIZE);
    bwrite(b1);
    brelse(b1);
    
    struct buf *b2 = bread(1, physical_block(disk2, blk));
    memmove(b2->data, src, BSIZE);
    bwrite(b2);
    brelse(b2);
  } else if (raid_policy == 5) {
    int stripe = logical_b / (NDISKS - 1);
    int parity_disk = stripe % NDISKS;
    int offset_in_stripe = logical_b % (NDISKS - 1);
    int data_disk = (parity_disk + 1 + offset_in_stripe) % NDISKS;
    int blk = stripe;

    // Read old data
    struct buf *old_b = bread(1, physical_block(data_disk, blk));
    // Read parity
    struct buf *p_b = bread(1, physical_block(parity_disk, blk));
    
    // Compute new parity
    for(int i = 0; i < BSIZE; i++) {
        p_b->data[i] ^= old_b->data[i] ^ src[i];
    }
    
    // Write parity
    bwrite(p_b);
    brelse(p_b);
    
    // Write new data
    memmove(old_b->data, src, BSIZE);
    bwrite(old_b);
    brelse(old_b);
  }
}

void raid_read(uint logical_b, char *dst) {
  if (raid_policy == 0) {
    int disk = logical_b % NDISKS;
    int blk = logical_b / NDISKS;
    struct buf *b = bread(1, physical_block(disk, blk));
    memmove(dst, b->data, BSIZE);
    brelse(b);
  } else if (raid_policy == 1) {
    int blk = logical_b / (NDISKS / 2);
    int disk1 = logical_b % (NDISKS / 2);
    // read from disk1
    struct buf *b1 = bread(1, physical_block(disk1, blk));
    memmove(dst, b1->data, BSIZE);
    brelse(b1);
  } else if (raid_policy == 5) {
    int stripe = logical_b / (NDISKS - 1);
    int parity_disk = stripe % NDISKS;
    int offset_in_stripe = logical_b % (NDISKS - 1);
    int data_disk = (parity_disk + 1 + offset_in_stripe) % NDISKS;
    int blk = stripe;

    if (data_disk == failed_disk) {
        // RECONSTRUCTION: Read all other data disks AND the parity disk in this stripe
        // XOR them all together to rebuild the lost data.
        for(int i = 0; i < BSIZE; i++) dst[i] = 0; // zero out destination
        
        for (int i = 0; i < NDISKS; i++) {
            if (i != failed_disk) {
                struct buf *b = bread(1, physical_block(i, blk));
                for(int j = 0; j < BSIZE; j++) {
                    dst[j] ^= b->data[j];
                }
                brelse(b);
            }
        }
    } else {
        struct buf *b = bread(1, physical_block(data_disk, blk));
        memmove(dst, b->data, BSIZE);
        brelse(b);
    }
  }
}

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
    uint base_block = slot * (PGSIZE / BSIZE);
    for (int i = 0; i < (PGSIZE / BSIZE); i++) {
        raid_write(base_block + i, src + i * BSIZE);
    }
}

void swap_read(int slot, char *dst) {
    uint base_block = slot * (PGSIZE / BSIZE);
    for (int i = 0; i < (PGSIZE / BSIZE); i++) {
        raid_read(base_block + i, dst + i * BSIZE);
    }
}