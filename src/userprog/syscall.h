#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "threads/interrupt.h"
#include <hash.h>

void syscall_init (void);
void exit (int, struct intr_frame *);
extern struct lock file_lock;   //lock for manipulating files in process
#ifdef VM
//----hash functions---//
unsigned mapid_hash (const struct hash_elem *p, void *aux);
bool mapid_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
//------------------------//
typedef int mapid_t;
#endif
typedef int pid_t;
struct file_map             //use to map fd to the real pointer to file for read/write
{

  struct list_elem elem;     //to store in list -> in struct thread
  struct file* filename;    //pointer to the file
  int file_id;              //fd of the open file - cannot be 0 or 1
};

struct mem_map         //memory file map - maps mapid_t to and open file
{
  struct hash_elem hashelem;
  int fd;            //fd as used in file_map to identify open files
  mapid_t mapid;     //unique map id for file
  void *start;        //start addr of the map
  void *end           //end addr of the map
};

#endif /* userprog/syscall.h */
