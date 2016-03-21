#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define INSTR_SIZE 4                       /* Instruction Size 4 bytes */
#define RET_ERROR -1                     
#define MAX_ARG_SIZE 128		   /* Maximum input size */  

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

void push_stack(void **, int);
struct child_process* get_child_process(tid_t child_tid, struct list *children_list);

#endif /* userprog/process.h */
