# PA3: Scheduler-Aware Page Replacement in xv6

## Implemented Features

- **Lazy allocation** (already in repo): `sbrk()` defers physical allocation until first access
- **Frame table**: global table tracking all resident user pages (owner, VA, PA)
- **Clock page replacement**: circular scan with reference bit (PTE_A) clearing
- **In-memory swap space**: fixed array of 512 slots × 4096 bytes each
- **Scheduler-aware eviction**: Clock prefers evicting pages from lower MLFQ priority processes
- **Per-process VM stats**: tracked in `struct proc`, exposed via `getvmstats(pid, *vmstats)`

---

## Design Decisions and Assumptions

**Frame limit (`NFRAMES = 128`)**  
Defined in `param.h`. All user page allocations go through `vmfault()`, which checks
`frames_full()` and calls `clock_evict()` before `kalloc()` if the limit is reached.

**Swap as in-memory array**  
`swap_data[NSWAP][PGSIZE]` lives in BSS. No disk I/O. A swap slot is allocated on eviction
and freed when the page is swapped back in or the process exits. Slot reuse is guaranteed
because swap-in always calls `swap_free()`.

**PTE encoding for swapped pages**  
When a page is evicted, its PTE is marked `PTE_V=0, PTE_SWAP=1` (bit 8, RSW field).
The swap slot index is stored in the PPN field. On the next access, `vmfault()` detects
this encoding and restores the page from swap before re-mapping it.

**Clock algorithm**  
Implemented in `clock_evict()` in `kalloc.c`. Scans the frame table circularly using
`clock_hand`. If `PTE_A` is set, it is cleared and the frame is skipped (second chance).
If `PTE_A` is 0, the frame is a candidate. Among candidates seen in one full scan, the
one belonging to the lowest MLFQ priority process (highest `mlfq_level` number) is
selected as the victim.

**`uvmunmap` fix**  
`frame_remove()` is called inside `uvmunmap` before `kfree()` so that frames are
correctly removed from the frame table when a process shrinks its address space or exits.
Swap slots for swapped-out pages are also freed in `uvmunmap` to prevent leaks.

**Locking**  
`frame_lock` protects the frame table and `nframes_used`. `swap_lock` protects
`swap_table`. These are always acquired separately and never nested to avoid deadlock.
`p->lock` is acquired briefly in `clock_evict()` to check process state before calling
`walk()`, preventing a kernel page fault on a freed pagetable.

**Assumptions**  
- Only user pages are tracked; kernel pages bypass the frame table entirely
- Swap space is large enough (512 slots) that it does not exhaust under normal workloads
- `NFRAMES = 128` is a soft cap enforced in software, not by hardware

---

## Experimental Results

Tests run with `NFRAMES=128`, `NSWAP=512`, `CPUS=3`.

### Test 1 — No eviction (64 pages)
Allocated 64 pages (half of NFRAMES). All faults are new-page faults, no eviction occurs.

```
page_faults=64  evicted=0  swapped_out=0  swapped_in=0  resident=64
```

Confirms the fast path in `vmfault()` works correctly and the frame table is not
prematurely triggering eviction.

### Test 2 — Eviction and data integrity (178 pages)
Allocated 178 pages (50 over NFRAMES). Clock evicted pages to make room.
All 178 pages read back correctly after the eviction/swap-in cycle.

```
page_faults=357  evicted=228  swapped_out=228  swapped_in=115  resident=128
```

The doubled fault count reflects each page being faulted in twice (write pass + read pass).
`resident` stayed at exactly 128, confirming the cap is enforced.

### Test 3 — Swap slot reuse (5 cycles)
Five alloc-access-free cycles over 148 pages each. Data correct in all cycles.
Confirms `swap_free()` is called on swap-in and slots are recycled. Without reuse,
5 × 20 = 100 slots would be permanently consumed.

### Test 4 — Invalid PID
`getvmstats(99999, &s)` returned `-1` as required.

### Test 5 — Scheduler-aware eviction
A CPU-bound child (burned 400M iterations, demoted to MLFQ level 3) and a
syscall-heavy child (called `getpid()` 3000 times, stayed at level 0) both
allocated `NFRAMES` pages. Parent then applied memory pressure.

```
cpu_evicted=18   sys_evicted=0
```

The CPU-bound child lost pages while the high-priority syscall-heavy child retained
its entire working set, confirming scheduler-aware eviction works correctly.