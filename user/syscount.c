#include "kernel/types.h"
#include "user/user.h"

int main()
{
  int initial_count, after_calls, child_pid;
  int parent_pid = getpid();
    
  initial_count = getsyscount();
  printf("Test 1: Initial syscall count: %d\n", initial_count);
  
  getpid();
  getpid();
  getpid();
  after_calls = getsyscount();
  printf("Test 2: After 3 getpid() calls: %d (increased by %d)\n", 
         after_calls, after_calls - initial_count);
    
  child_pid = fork();
  
  if(child_pid == 0) {
    // Child process
    int child_count = getsyscount();
    printf("Child was created\n");
    printf("Test 3: Child's initial syscall count: %d\n", child_count);
    
    int result = getchildsyscount(parent_pid);
    printf("Child trying reading parent's count (should fail): %d\n", result);
    if(result == -1) {
      printf("Child cannot read parent's count\n");
    } else {
      printf("parent's system call count = %d (This isn't expected behaviour)\n",result);
    }
    
    printf("Child finished\n");
    exit(0);
  } else {
    // Parent process
    pause(10);  // Let child run
    
    printf("\nTest 4: Parent reading child's syscall count\n");
    int child_count = getchildsyscount(child_pid);
    if(child_count==-1){
        printf("Given PID is not a child\n");
    }
    else{
      printf("Child PID %d has made %d syscalls\n", child_pid, child_count);
    }
    
    printf("\nTest 5: Reading non-existent child (PID 9999)\n");
    int invalid = getchildsyscount(9999);
    printf("Result: %d\n", invalid);
    if(invalid == -1) {
      printf("Correctly returned -1 for invalid PID\n");
    } else {
      printf("Didn't return -1. Invalid behaviour\n");
    }
    
    printf("\nTest 6: Parent reading its own PID (using getchildsyscount)\n");
    int self = getchildsyscount(parent_pid);
    printf("  Result: %d\n", self);
    if(self == -1) {
        printf("Given PID is not a child\n");
    } else {
      printf("didn't return -1 for own PID (unexpected behaviour)\n");
    }
    
    printf("\nTest 7: Multiple children test\n");
    int child2_pid = fork();
    if(child2_pid == 0) {
      // Second child
      pause(5);
      exit(0);
    }
    
    pause(10);
    int child1_syscalls = getchildsyscount(child_pid);
    int child2_syscalls = getchildsyscount(child2_pid);
    printf("  Child 1 (PID %d): %d syscalls\n", child_pid, child1_syscalls);
    printf("  Child 2 (PID %d): %d syscalls\n", child2_pid, child2_syscalls);
    
    printf("\nTest 8: Parent's final syscall count (using getsyscount)\n");
    int parent_final = getsyscount();
    printf("  Parent made %d syscalls total\n", parent_final);
    printf("  Increased by %d since start\n", parent_final - initial_count);
    
    wait(0);
    wait(0);
    printf("\nNumbers are high because each printf,fork,wait,pause etc make many system calls internally.\n");
  }
  
  exit(0);
}