// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "proc.h"
#include "swap.h"

struct frame_entry {
    int  in_use;
    struct proc *proc;
    uint64 va;
    uint64 pa;     
};

struct frame_entry frame_table[NFRAMES];
int clock_hand = 0;
struct spinlock frame_lock;
int nframes_used = 0;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void frameinit(void) {
    initlock(&frame_lock, "frame");
    for (int i = 0; i < NFRAMES; i++)frame_table[i].in_use = 0;
}

void frame_add(struct proc *p, uint64 va, uint64 pa) {
    acquire(&frame_lock);
    for (int i=0;i<NFRAMES;i++) {
        if (!frame_table[i].in_use) {
            frame_table[i].in_use=1;
            frame_table[i].proc=p;
            frame_table[i].va=va;
            frame_table[i].pa=pa;
            nframes_used++;
            release(&frame_lock);
            return;
        }
    }
    release(&frame_lock);
    panic("frame_add: no free slot");
}

void frame_remove(uint64 pa) {
    acquire(&frame_lock);
    for (int i=0;i<NFRAMES;i++) {
        if(frame_table[i].in_use && frame_table[i].pa==pa) {
            frame_table[i].in_use=0;
            frame_table[i].proc->resident_pages--; 
            nframes_used--;
            release(&frame_lock);
            return;
        }
    }
    release(&frame_lock);
}

void frame_remove_proc(struct proc *p) {
    acquire(&frame_lock);
    for (int i=0;i<NFRAMES;i++) {
        if(frame_table[i].in_use && frame_table[i].proc==p) {
            frame_table[i].in_use=0;
            nframes_used--;
        }
    }
    release(&frame_lock);
}

int frames_full(void) {
    acquire(&frame_lock);
    int full=(nframes_used >= NFRAMES);
    release(&frame_lock);
    return full;
}

uint64 clock_evict(void) {
    extern pte_t *walk(pagetable_t, uint64, int);

    acquire(&frame_lock);

    int best=-1;
    int scans=0;

    while(scans<NFRAMES*2) {
        int i=clock_hand%NFRAMES;
        clock_hand=(clock_hand+1)%NFRAMES;
        scans++;

        if (!frame_table[i].in_use)continue;

        struct proc *p=frame_table[i].proc;
        uint64 va=frame_table[i].va;

        acquire(&p->lock);
        int alive=(p->state!=UNUSED && p->state!=ZOMBIE&& p->pagetable!=0);
        release(&p->lock);
        if (!alive) {
            frame_table[i].in_use = 0;
            nframes_used--;
            continue;
        }
        pte_t *pte = walk(p->pagetable, va, 0);
        if (pte == 0) continue;

        if (*pte & PTE_A) {
            *pte &= ~PTE_A;
            continue;
        }

        if (best == -1 || p->mlfq_level > frame_table[best].proc->mlfq_level)best = i;

        if (scans >= NFRAMES)break;
    }

    if (best == -1) {
        release(&frame_lock);
        return 0;
    }

    struct proc *vp  = frame_table[best].proc;
    uint64       vva = frame_table[best].va;
    uint64       vpa = frame_table[best].pa;
    frame_table[best].in_use = 0;
    nframes_used--;
    release(&frame_lock);

    pte_t *pte = walk(vp->pagetable, vva, 0);
    if (pte == 0 || !(*pte & PTE_V)) return 0;

    int slot = swap_alloc(vp->pid, vva);
    if (slot < 0) panic("clock_evict: swap full");

    swap_write(slot, (char *)vpa);
    *pte = MAKE_SWAP_PTE(slot);
    sfence_vma();

    vp->pages_evicted++;
    vp->pages_swapped_out++;
    vp->resident_pages--;

    kfree((void *)vpa);
    return vpa;
}