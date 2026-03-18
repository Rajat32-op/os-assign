# xv6 System Call Implementation

## Implemented System Calls

### 1. `hello()`
**Description:** Prints "Hello from the kernel!" to the kernel console.

**Purpose:** Demonstrates basic syscall implementation without arguments or return values.

**Usage:**
```c
hello();
```

**Testing:**
```bash
$ hello
```

### 2. `getpid2()`
**Description:** Returns the Process ID (PID) of the calling process (without using inbuilt getpid function).

**Return Value:**
- Process PID on success

**Usage:**
```c
int pid = getpid2();
```

**Testing:**
```bash
$ getpid2
```

---

### 3. `getppid()`
**Description:** Returns the Process ID (PID) of the calling process's parent. XV6 reparents orphan processes to init process. so except init process, every process has a parent

**Return Value:**
- Parent's PID on success
- `-1` if parent is NULL (only case: init process)

**Usage:**
```c
int parent_pid = getppid();
```

**Testing:**
```bash
$ getppid
```

---

### 4. `getnumchild()`
**Description:** Returns the number of currently alive child processes of the calling process.

**Behavior:**
- Zombie processes are NOT counted as they are considered dead
- Traverses entire process table to find children

**Return Value:** Count of alive children (≥ 0)

**Usage:**
```c
int num_children = getnumchild();
```

**Testing:**
```bash
$ getnumchild
```

---

### 5. `getsyscount()`
**Description:** Returns the total number of system calls made by the calling process since its creation.

**Return Value:** Total syscall count (≥ 0)

**Usage:**
```c
int count = getsyscount();
```

**Testing:**
```bash
$ syscount
```
(tests both getsyscount and getchildsyscount)

---

### 6. `getchildsyscount(int pid)`
**Description:** Returns the total number of system calls made by a child process.

**Parameters:**
- `pid`: Process ID of the child to query

**Return Value:**
- Child's syscall count on success
- `-1` if the given PID is not a child of the calling process
- `-1` if the given PID does not exist
- `-1` if the given PID is the calling process itself

**Usage:**
```c
int child_syscalls = getchildsyscount(child_pid);
if (child_syscalls == -1) {
    printf("Error: PID is not a child\n");
}
```

**Testing:**
```bash
$ syscount
```
(tests both getsyscount and getchildsyscount)

---

## Design Decisions and Assumptions

### 1. Process Table Traversal
**Decision:** Used pointer-based iteration pattern `for(p = proc; p < &proc[NPROC]; p++)`

**Reason:** This is the standard way used throughout xv6 kernel code (e.g., in `proc.c`). It's efficient and consistent with existing codebase conventions.

### 2. Locking Strategy
**Decision:** Acquire individual process locks before reading state fields.

**Implementation:**
```c
for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    // Read p->state, p->parent, p->syscount
    release(&p->lock);
}
```

**Reason:** Prevents race conditions where process state could change during reads. Each lock is held only briefly to minimize contention.

### 3. Zombie Process Handling
**Decision:** `getnumchild()` does NOT count ZOMBIE processes.

**Reason:** 
- Zombies are dead processes waiting to be reaped
- Counting them would misrepresent active children

### 4. Syscall Counter Storage
**Decision:** Added `int syscount` field to `struct proc` in `proc.h`.

**Location:** Placed with other per-process state variables (after `pid` field).

**Initialization:** Set to `0` in `allocproc()` when new process is created.

**Increment Point:** Counter incremented in `syscall()` function BEFORE dispatching to handler.

### 5. Syscall Numbering
**Decision:** Assigned sequential syscall numbers starting from 22.

**Mapping:**
- `SYS_hello = 22`
- `SYS_getppid = 23`
- `SYS_getnumchild = 24`
- `SYS_getsyscount = 25`
- `SYS_getchildsyscount = 26`

**Reason:** Next available numbers after existing syscalls, maintaining sequential order.

### 6. External Declaration of Process Table
**Decision:** Added `extern struct proc proc[NPROC];` in files needing access.

**Reason:**
- Process table `proc[]` is defined in `proc.c` but not declared in any header
- Needed for syscalls that traverse process table
- Follows xv6 convention of selective exposure

### 7. Test Coverage
**Assumption:** `printf()` generates multiple `write()` syscalls internally.

**Observation:** A single `printf()` call results in 10-20 syscalls depending on output length.

**Impact:** Syscall counts appear high but are correct. This reveals the hidden cost of I/O operations.

---

## Files Modified

### Kernel Files
- `kernel/proc.h` - Added `syscount` field to `struct proc`
- `kernel/proc.c` - Initialized `syscount` in `allocproc()`
- `kernel/syscall.h` - Added syscall number definitions
- `kernel/syscall.c` - Incremented counter, added extern declarations and syscall table entries
- `kernel/sysproc.c` - Implemented all syscall handlers

### User Files
- `user/user.h` - Added syscall function declarations
- `user/usys.pl` - Added syscall stub generation entries
- `user/hello.c` - Test program for `hello()`
- `user/getnumchild.c` - Test program for `getnumchild()`
- `user/syscount.c` - Comprehensive test suite for syscall counters

### Build System
- `Makefile` - Added user test programs to `UPROGS`

---
