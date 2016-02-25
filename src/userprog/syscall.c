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
  thread_exit();    //in thread.c which calls process_exit() in process.c
    //send status to kernel ?????????? HOWWWWWWWWWWWWWWWWWWWWW
  return;
}

int 
wait (pid_t pid){
  return -999;
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
      printf ("HERE\n");
      break;
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
	putbuf(buffer, wsize);      //if goes to if clause above this prints last section of buffer which is < maxbufout
	return size;
      default :
	//return file_write( 'convert fd to file* here', buffer, size);  //in file.c
	break;
   }
}

  
  
  
  
  

