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
    case SYS_EXIT:
      exit(args[0]);   //think this is the right arg for exit -> return afterwards?
      break;
    case SYS_WRITE:
      write(args[0], args[1], args[2]);
      break;
    case default: // original code
      printf("syscall!");
      thread_exit ();
  }

  //------- write your methods here --------//
  void
  exit (int status){
    thread_exit();    //in thread.c which calls process_exit() in process.c
    //send status to kernel ?????????? HOWWWWWWWWWWWWWWWWWWWWW
    return;
  }
  
//putbuf() in specs. lib/kernel/console.c via stdio.h***
  int
  write (int fd, const void *buffer, unsigned size) {
    //maybe check if buffer pointer is valid here
    char* data;
    data = buffer;
    unsigned wsize = size;
    switch(fd){
      case 1 : 			//write to sys console
	const int maxbufout = 300;
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
      case default :
	//return file_write( 'convert fd to file* here', buffer, size);  //in file.c
	break;
    }
  }
  
  
  
  
  
  
}
