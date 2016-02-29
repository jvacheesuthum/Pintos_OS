#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "threads/interrupt.h"

void syscall_init (void);
void exit (int, struct intr_frame *);
extern struct lock file_lock;   //lock for manipulating files in process

typedef int pid_t;
struct file_map             //use to map fd to the real pointer to file for read/write
{
  struct list_elem elem;     //to store in list -> in struct thread
  struct file* filename;    //pointer to the file
  int file_id;              //fd of the open file - cannot be 0 or 1
};

#endif /* userprog/syscall.h */
