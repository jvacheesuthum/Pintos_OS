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
  int syscall_name = *(int *)esp; 

  switch(syscall_name){
    case SYS_EXIT:
      printf("before exit in switch");
      exit(*(int *)(esp + 4));   //think this is the right arg for exit -> return afterwards?
      printf("after exit in switch");
      break;
    case SYS_WRITE:
     write(*(int *)(esp + 4),* (char **)(esp + 4*2),* (unsigned *)(esp + 4*3));
      break;
    /*case SYS_HALT:
      halt();
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
    //case SYS_WRITE:
     // break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
*/
    default: // original code*/
      printf("syscall!: %i\n", syscall_name);
     // thread_exit ();
  }
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
  struct list exit_statuses = (thread_current() -> parent_process) -> children_process;  
  struct list_elem* e;
  e = list_begin (&exit_statuses);
  while (e != list_end (&exit_statuses)) {
    struct child_process *cp = list_entry (e, struct child_process, elem);
    if(cp->tid == thread_current()->tid){
      cp -> exit_status = status;
      break;
    }
    e = list_next(e);
  }
  thread_exit();    //in thread.c which calls process_exit() in process.c
}

int 
wait (pid_t pid){
  /* All of a process’s resources, including its struct thread, must be freed whether its parent ever waits for it or not, and regardless of whether the child exits before or after its parent.?? FROM SPEC PG 31 */
  //cleaning up happens in process_exit() but can't find where struct thread is cleaned up ????
  
  return process_wait((tid_t) pid);
}
  
//putbuf() in specs. lib/kernel/console.c via stdio.h***
int
write (int fd, const void *buffer, unsigned size) {
  //maybe check if buffer pointer is valid here
  char* data;
  data =  buffer;
  unsigned wsize = size;
  unsigned maxbufout;
  switch(fd){
    case 0 :
    	return -1;              //fd 0 is standard in, cannot be written
    case 1 : 			//write to sys console
	/*maxbufout = 300;
	if (size > maxbufout) { 	//break up large buffer (>300 bytes for now)
	  unsigned wsize = size;
	  char* temp;
	  while (wsize > maxbufout) {
	    strlcpy(temp, data, maxbufout);
	    putbuf(temp, maxbufout);		       
	    data += maxbufout;       //cuts the first 300 char off data
	    wsize -= maxbufout;
	  }
	}	*/
	putbuf(buffer, wsize);      
	return size;
    default :
	struct file_map* target = get_file_map(fd);
	return file_write(target-> filename, buffer, size);  //defined in file.c
	break;
    }
  }
    
    //NEEDS LOCK where a file is involve ?
int
open (const char *file) {
  if (file == NULL) return -1;
  struct file* opening = filesys_open(file); 
  if (opening == NULL) return -1;
      
  //map the opening file to an available fd (not 0 or 1) and returns fd
  struct file_map* newmap;
  int newfile_id = thread_current() -> next_fd;
  assert(newfile_id > 1);
  thread_current() -> next_fd ++;    //increment next available descriptor
  newmap -> filename = opening;
  newmap -> file_id = newfile_id;
  list_push_back(thread_current()-> files, newmap-> elem); //put this fd-file map into list in struct thread
  return newfile_id;
}
    
    
    
void
close (int fd) {
  struct file_map
}
    
    //----------utility fuctions---------------//
    
    //takes file descriptor and returns pointer to the file map that corresponds to it
    struct file_map*
    get_file_map(int fd) { 
      struct list files = thread_current()-> files;
      struct list_elem e*;
      for (e = list_begin(&files); e != list_end (&files); e = list_next (e)) {
        struct file_map *map = list_entry (e, struct file_map, elem);
        if (map->fid == fid){
          return map;
        } 
      }
      return NULL;
    }
  }

  
  
  
  
  

