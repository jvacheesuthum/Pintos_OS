#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "devices/shutdown.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

static void halt (void);
static void exit (int status);
static pid_t exec(const char *cmd_line);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  /*
   * The caller’s stack pointer is accessible to syscall_handler() as the ‘esp’ member 
   * of the struct intr_frame passed to it. (struct intr_frame is on the kernel stack.)
   */

  // refer to page37 specs // or use esp as int* sys_name as seen in lib/user/syscall.c
  void *esp   = f->esp;
  int argc    = *(*esp + 4); //check if +4 is correct? the address for argc is at address esp + 4 
  char** argv = *(*esp + 4*2);
  char* syscall_name = *argv; 
  char* args[argc];
  int i;
  for(i=0; i<argc; i++){
    args[i] = *(argv + (i+1)*4);
  }

  switch(syscall_name){
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit();
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
    case default: // original code
      printf("syscall!");
      thread_exit ();
  }

  //------- write your methods here --------//
  void
  halt(void){
    shutdown_power_off();
  }

  pid_t exec(const char *cmd_line){
    //TODO: add synchronization
    tid_t pid = process_execute(cmd_line); 
    //taking pid as tid, both are ints
    return (pid_t) pid;  
  }

  void
  exit (int status){
    return;
  }

  int 
  wait (pid_t pid){
    return -999;
  }
  
  int
  write (int fd, const void *buffer, unsigned size) {
    return 0;
  }

  
  
  
  
  
}
