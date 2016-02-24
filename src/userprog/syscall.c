#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

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
    case syscall_name == SYS_EXIT:
      exit();
      break;
    case default: // original code
      printf("syscall!");
      thread_exit ();
  }
}
