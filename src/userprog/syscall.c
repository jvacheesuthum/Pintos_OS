#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame *);


static void halt (void);
static void exit (int status, struct intr_frame *f);
//static void exit (int status);
static pid_t exec(const char *cmd_line);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file, struct intr_frame *f);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position, struct intr_frame *f);
static unsigned tell (int fd, struct intr_frame *f);
static void close (int fd, struct intr_frame *f);
struct file_map* get_file_map(int fd); 

static struct lock file_lock;   //lock for manipulating files in process

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
  void *esp   = pagedir_get_page(thread_current()->pagedir, f->esp);
  int syscall_name = *(int *)esp; 
  lock_init(&file_lock);

  switch(syscall_name){
    case SYS_EXIT:
      exit(*(int *) (esp + 4),f);
      break;
    case SYS_WRITE:
      f->eax = write(*(int *)(esp + 4),* (char **)(esp + 4*2),* (unsigned *)(esp + 4*3));
      break;
    case SYS_HALT:
      halt();
      break;
    case SYS_EXEC:
      f->eax = exec (*(char **) (esp + 4));
      break;
    case SYS_WAIT:
      f->eax = wait (* (int *)(esp + 4));
      break;
    case SYS_CREATE:
      f->eax = create(*(char **) (esp + 4), *(unsigned *) (esp + 4*2));
      break;
    case SYS_REMOVE:
      f->eax = remove(*(char **) (esp + 4),f);
      break;
    case SYS_OPEN:
      f->eax = open(*(char **) (esp + 4*1));
      break;
    case SYS_FILESIZE:
      f->eax = filesize(*(int *) (esp + 4)); 
      break;
    case SYS_READ:
      //f->eax = read(*(int *)(esp + 4),* (char **)(esp + 4*2),* (unsigned *)(esp + 4*3));
      break;
    case SYS_SEEK:
      seek(*(int *)(esp + 4), *(unsigned *)(esp + 4*2), f);
      break;
    case SYS_TELL:
      f->eax = tell(*(int *)(esp + 4), f);
      break;
    case SYS_CLOSE:
      close(*(int *) (esp + 4), f); 
      break;

    default: // original code*/
//      printf("syscall!: %i\n", syscall_name);
//      thread_exit ();
      break;
  }
}
  //------- write your methods here --------//
static void
halt(void){
  shutdown_power_off();
}

static pid_t
exec(const char *cmd_line){
  lock_acquire(&file_lock);
  tid_t pid = process_execute(cmd_line);
  lock_release(&file_lock);
  //deny writes to this file is in start_process and process_exit
  //taking pid as tid, both are ints
  return (pid_t) pid;  
}

/*
static
void
exit (int status, struct intr_frame *f){
 
  if(thread_current() -> parent_process == NULL){
    printf("exit syscall: parent is null \n");
  }
  else{
    struct list exit_statuses = (thread_current() -> parent_process) -> children_process;  
    struct list_elem* e;
    e = list_begin (&exit_statuses);
    while (e != list_end (&exit_statuses)) {
      struct child_process *cp = list_entry (e, struct child_process, elem);
//      if(cp->tid == thread_current()->tid){
//        cp -> exit_status = status;
//        printf(cp);
        break;
//      }
      e = list_next(e);
=======*/
static void
exit (int status, struct intr_frame* f){
  f->eax = status;
  thread_current()->exit_status = status;
  thread_exit();

}

static int 
wait (pid_t pid){
  /* All of a process’s resources, including its struct thread, must be freed whether its parent ever waits for it or not, and regardless of whether the child exits before or after its parent.?? FROM SPEC PG 31 */
  //cleaning up happens in process_exit() but can't find where struct thread is cleaned up ????
  return process_wait((tid_t) pid);
}
  
static bool 
create (const char *file, unsigned initial_size)
{
  if (file == NULL || initial_size == NULL) return -1;
  return filesys_create(file, initial_size);
}

static bool
remove (const char *file, struct intr_frame* f) {
  if (file == NULL) exit(-1, f);
  lock_acquire(&file_lock);
  bool remove = filesys_remove(file);
  lock_release(&file_lock);
  return remove;
}

static int
filesize(int fd) {
  lock_acquire(&file_lock);
  struct file_map* map = get_file_map(fd);
  if (map == NULL) return -1;
  int length = file_length(map-> filename);
  lock_release(&file_lock);
  return length;
}

static int 
read (int fd, void *buffer, unsigned size)
{
  unsigned count = 0;
  struct file_map *file_map;
  switch(fd) {
    case 0:
      while (count != size) {
        *(uint8_t *)(buffer + count) = input_getc();
        count++;
      }
      if (count == size) {
        return size;
      } else {
        return count;
      }
    case 1:
      return -1;
    default:
      file_map = get_file_map(fd);
      if (file_map == NULL) {
        return -1;
      }
      lock_acquire(&file_lock);
      int read = file_read (file_map->filename, buffer, size);
      lock_release(&file_lock);
      return read;
  }
}

static int
write (int fd, const void *buffer, unsigned size) {
  //maybe check if buffer pointer is valid here
  char* data;
  data = buffer;
  unsigned wsize = size;
  unsigned maxbufout;
  struct file_map* target;
  switch(fd){
    case 0 :
    	return -1;              //fd 0 is standard in, cannot be written
    case 1 : 			//write to sys console
	maxbufout = 300;
	if (size > maxbufout) { 	//break up large buffer (>300 bytes for now)
	  unsigned wsize = size;
	  char* temp;
	  while (wsize > maxbufout) {
	    strlcpy(temp, data, maxbufout);
	    putbuf(temp, maxbufout);		       
	    data += maxbufout;       //cuts the first 300 char off data
	    wsize -= maxbufout;
	  }
	}
	putbuf(buffer, wsize);      
	return size;
    default :
	target = get_file_map(fd);
	if (target == NULL) return -1;
	lock_acquire(&file_lock);
	int write = file_write(target-> filename, buffer, size);  //defined in file.c
	lock_release(&file_lock);
	return write;
    }
  }
    
static int
open (const char *file) {
  if (file == NULL) return -1;
  lock_acquire(&file_lock);
  struct file* opening = filesys_open(file); 
  lock_release(&file_lock);
  if (opening == NULL) return -1;
      
  //map the opening file to an available fd (not 0 or 1) and returns fd
  struct file_map* newmap = (struct file_map *) malloc (sizeof(struct file_map));
  int newfile_id = thread_current() -> next_fd;
  thread_current() -> next_fd ++;    //increment next available descriptor
  newmap -> filename = opening;
  newmap -> file_id = newfile_id; 
  list_push_back(&thread_current()-> files, &newmap-> elem); //put this fd-file map into list in struct thread
  return newfile_id;
}
    
static void
seek (int fd, unsigned position, struct intr_frame* f) {
  struct file_map* map = get_file_map(fd);
  if (map == NULL) exit(-1,f);
  lock_acquire(&file_lock);
  file_seek(map-> filename, position);
  lock_release(&file_lock);
}

static unsigned
tell (int fd, struct intr_frame* f) {
  struct file_map* map = get_file_map(fd);
  if (map == NULL) exit(-1,f);
  lock_acquire(&file_lock);
  int tell = file_tell(map-> filename);
  lock_acquire(&file_lock);
  return tell;
}    
    
static void
close (int fd, struct intr_frame* f) {
  struct file_map* map = get_file_map(fd);
  if (map == NULL) exit(-1,f);
  lock_acquire(&file_lock);
  list_remove(&map-> elem);
  file_close(map-> filename);
  //might have to free the file/inode ????
  lock_release(&file_lock);
}
    
//----------utility fuctions---------------//
  
//takes file descriptor and returns pointer to the file map that corresponds to it
//not sure if it needs lock -> no locks acquired during calls to this
struct file_map*
get_file_map(int fd) { 
  struct thread *t = thread_current();
  struct list_elem *e;
  
  for (e = list_begin(&t->files); e != list_end (&t->files); e = list_next(e)) {
    struct file_map *map = list_entry (e, struct file_map, elem);
    if (map->file_id == fd) {
      return map;
    }
  }
  return NULL;
}
 

  
  
  
  
  

