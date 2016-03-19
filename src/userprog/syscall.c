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
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/string.h"

static void syscall_handler (struct intr_frame *);


static void halt (void);
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
static mapid_t mmap (int fd, void *addr);
static void munmap (mapid_t mapping);
struct file_map* get_file_map(int fd); 
struct lock file_lock; 


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
  void *esp = pagedir_get_page(thread_current()->pagedir, f->esp);
  if (esp == NULL || 
     esp < PHYS_BASE) {
       exit(RET_ERROR, f);
  }
  int syscall_name = *(int *)esp;
  if (syscall_name < SYS_HALT || 
      syscall_name > SYS_INUMBER) 
	exit(RET_ERROR, f);

  int* intptr;
  char** charptr;
  unsigned* uptr;
  void* endofbuff;

  switch(syscall_name){
    case SYS_EXIT:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (intptr == NULL) exit(RET_ERROR, f);
      exit(*(intptr),f);
      break;
    case SYS_WRITE:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      charptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*2);
      uptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*3);
      endofbuff = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*2 + *uptr);
      if (intptr == NULL || charptr == NULL || uptr == NULL || endofbuff == NULL) exit(RET_ERROR, f);
      f->eax = write(*(intptr), *(charptr), *(uptr));
      break;
    case SYS_HALT:
      halt();
      break;
    case SYS_EXEC:
      charptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (charptr == NULL) exit(RET_ERROR, f);
      f->eax = exec (*(charptr));
      break;
    case SYS_WAIT:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (intptr == NULL) exit(RET_ERROR, f);
      f->eax = wait (* (intptr));
      break;
    case SYS_CREATE:
      charptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      uptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*2);
      if (charptr == NULL || uptr == NULL) exit(RET_ERROR, f);
      f->eax = create(*(charptr), *(uptr));
      break;
    case SYS_REMOVE:
      charptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (charptr == NULL) exit(RET_ERROR, f);
      f->eax = remove(*(charptr),f);
      break;
    case SYS_OPEN:
      charptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (charptr == NULL) exit(RET_ERROR, f);
      f->eax = open(*(charptr));
      break;
    case SYS_FILESIZE:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (intptr == NULL) exit(RET_ERROR, f);
      f->eax = filesize(*(intptr)); 
      break;
    case SYS_READ:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      charptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*2);
      uptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*3);
      endofbuff = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*2 + *uptr);
      if (intptr == NULL || charptr == NULL || uptr == NULL || endofbuff == NULL) exit(RET_ERROR, f);
      f->eax = read(*(intptr), *(charptr), *(uptr));
      break;
    case SYS_SEEK:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      uptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE*2);
      if (intptr == NULL || uptr == NULL) exit(RET_ERROR, f);
      seek(*(intptr), *(uptr), f);
      break;
    case SYS_TELL:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (intptr == NULL) exit(RET_ERROR, f);
      f->eax = tell(*(intptr), f);
      break;
    case SYS_CLOSE:
      intptr = pagedir_get_page(thread_current()->pagedir, f->esp + INSTR_SIZE);
      if (intptr == NULL) exit(RET_ERROR, f);
      close(*(intptr), f); 
      break;

    default:
      break;
  }
}

static void
halt(void){
  shutdown_power_off();
}

static pid_t
exec(const char *cmd_line){
  /*process_execute does synchronisation, so don't need a lock here*/
  tid_t pid = process_execute(cmd_line);
  //deny writes to this file is in start_process and process_exit
  //taking pid as tid, both are ints
  return (pid_t) pid;  
}

void
exit (int status, struct intr_frame* f){
  if(f != NULL) f->eax = status;
  struct thread *parent = thread_current()->parent_process;
  struct child_process *pcp = get_child_process(thread_current()->tid, &parent->children_processes);
  if(pcp->exit_status != NULL){
    // it might be NULL when parent has already exited and free is called on its child processes
    pcp->exit_status = status;
  }

  printf("%s: exit(%i)\n", thread_current()->name, status);
  thread_exit();
}

static int 
wait (pid_t pid){
  /* All of a process’s resources, including its struct thread, must be freed whether its parent ever waits for it or not, and regardless of whether the child exits before or after its parent.?? FROM SPEC PG 31 */
  return process_wait((tid_t) pid);
}
  
static bool 
create (const char *file, unsigned initial_size)
{
  if (file == NULL) exit(-1, NULL);
  
  lock_acquire(&file_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return result;
}

static bool
remove (const char *file, struct intr_frame* f) {
  if (file == NULL) exit(RET_ERROR, f);
  lock_acquire(&file_lock);
  bool remove = filesys_remove(file);
  lock_release(&file_lock);
  return remove;
}

static int
filesize(int fd) {
  lock_acquire(&file_lock);
  struct file_map* map = get_file_map(fd);
  if (map == NULL) return RET_ERROR;
  int length = file_length(map-> filename);
  lock_release(&file_lock);
  return length;
}

static int 
read (int fd, void *buffer, unsigned size)
{
  //check if in user addr space
  if (!is_user_vaddr (buffer) || 
      !is_user_vaddr (buffer + size)) {
    exit(RET_ERROR, NULL);
  }
  unsigned count = 0;
  struct file_map *file_map;
  switch(fd) {
    case 0://stdin
      lock_acquire(&file_lock);
      while (count != size) {
        *(uint8_t *)(buffer + count) = input_getc();
        count++;
      }
      lock_release(&file_lock);
      return count;
    case 1:
      return RET_ERROR;
    default: //read from file
      file_map = get_file_map(fd);
      if (file_map == NULL) {
        return RET_ERROR;
      }
      lock_acquire(&file_lock);
      int read = file_read (file_map->filename, buffer, size);
      lock_release(&file_lock);
      return read;
  }
}

static int
write (int fd, const void *buffer, unsigned size) {

  if (!is_user_vaddr (buffer) || 
      !is_user_vaddr (buffer + size)) {
    exit(RET_ERROR, NULL);
  }

  char* data;
  data = buffer;
  unsigned wsize = size;
  unsigned maxbufout;
  struct file_map* target;
  switch(fd){
    case 0 :
      return RET_ERROR;  //fd 0 is standard in, cannot be written
    case 1 : //write to sys console
	maxbufout = 300;
	if (size > maxbufout) { //break up large buffer (>300 bytes for now)
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
	if (target == NULL) return RET_ERROR;
	lock_acquire(&file_lock);
	int write = file_write(target-> filename, buffer, size);  //defined in file.c
	lock_release(&file_lock);
	return write;
    }
  }
    
static int
open (const char *file) {
  if (file == NULL) return RET_ERROR;

  lock_acquire(&file_lock);
  struct file* opening = filesys_open(file); 
  lock_release(&file_lock);
  if (opening == NULL) return RET_ERROR;

  //map the opening file to an available fd (not 0 or 1) and returns fd
  struct file_map* newmap = (struct file_map *) malloc (sizeof(struct file_map));
  if (newmap == NULL){
     free(newmap);
     return RET_ERROR;
  }
  lock_acquire(&file_lock);
  int newfile_id = thread_current() -> next_fd;
  thread_current() -> next_fd ++;    //increment next available descriptor
  newmap -> filename = opening;
  newmap -> file_id = newfile_id; 
  list_push_back(&thread_current()-> files, &newmap-> elem);
  //put this fd-file map into list in struct thread
  lock_release(&file_lock);
  return newfile_id;
}
    
static void
seek (int fd, unsigned position, struct intr_frame* f) {
  struct file_map* map = get_file_map(fd);
  if (map == NULL) exit(RET_ERROR,f);
  lock_acquire(&file_lock);
  file_seek(map-> filename, position);
  lock_release(&file_lock);
}

static unsigned
tell (int fd, struct intr_frame* f) {
  struct file_map* map = get_file_map(fd);
  if (map == NULL) exit(RET_ERROR,f);
  lock_acquire(&file_lock);
  int tell = file_tell(map-> filename);
  lock_acquire(&file_lock);
  return tell;
}    
    
static void
close (int fd, struct intr_frame* f) {
  struct file_map* map = get_file_map(fd);
  if (map == NULL) exit(RET_ERROR,f);
  lock_acquire(&file_lock);
  list_remove(&map-> elem);
  file_close(map-> filename);
  free(map);
  lock_release(&file_lock);
}

static mapid_t
mmap (int fd, void *addr) {
  if (addr == NULL || !addr || addr % PGSIZE == 0 || fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return -1;
  }
  lock_acquire(&file_lock);
  struct file* opening = file_reopen(get_file_map(fd)-> filename); 
  lock_release(&file_lock);
  int size = file_size(opening);
  if (!size || opening == NULL) {  
    return -1;
  }
  void* original_addr = addr;
  //loop to write 1 pgsize at a time to addr, use addr += pgsize to move on
  //check out spec 5.3.4 for "stick out" part
  while (size > 0) {
    if () { //mapped overlaps existing pages -> retrieve the addr and see if there anything in there - HOW
      return -1;
    }
    if (size > PGSIZE) {
      //TODO write to one page at addr
      addr += PGSIZE
      size -= PGSIZE
    } else {
    
    }
  }  
  struct mem_map *memmap = (struct mem_map *) malloc (sizeof (struct mem_map));
  if (memmap == NULL) {
    return -1;
  }
  memmap-> start = original_addr;
  memmap-> end = addr;
  memmap-> fd = fd
  memmap-> mapid = next_mapid;             //TODO define this somewhere
  next_mapid++;
  hash_insert(mmap_table, memmap-> hashelem); //TODO define the hash - mmap_table and init somewhere
}
static void
munmap (mapid_t mapping) {
  struct mem_map *map = get_mem_map(mapping);
  if (map == NULL) return;          //or exit -1 here?
  void* start = map-> start;
  void* end = map-> end;
  for (start; start < end; start += PGSIZE) {
    void* pg_addr = pagedir_get_page(thread_current()-> supp_pagetable-> pagedir, map->start)
    //TODO ^^^ replace supppagetable with real one in thread.h
    if (pg_addr == NULL) {
      continue;              //page might already be freed by some other method, keep checking until the end
    } else {
      free(pg_addr);
    }
  }
  hash_delete(mmap_table, map-> hashelem);
  free(map);
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

//takes a mapid and returns struct *mem_map that has the corresponding mapid
struct mem_map*
get_mem_map (mapid_t mapping) {
  struct mem_map map;
  //ref appendix A page 75 for searching
  struct hash_elem *elem 
  map.mapid = mapping
  elem = hash_find (&mmap_table, &map.hash_elem);
  return elem != NULL ? hash_entry (elem, struct mem_map, hash_elem) : NULL;
}
 

  
  
  
  
  

