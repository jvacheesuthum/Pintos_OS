#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid = TID_ERROR;

  char *fn_copy2, *save_ptr;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
 
  //-----------ARGPASS-------------------//
  //Get the file name for thread create
  fn_copy2 = malloc (strlen(file_name) + 1);
  if (fn_copy2 == NULL) {
    palloc_free_page (fn_copy);
      return TID_ERROR;
   }
//  strlcpy (fn_copy2, file_name, PGSIZE);
  memcpy (fn_copy2, file_name, strlen(file_name) + 1);
  file_name = strtok_r (fn_copy2, " ", &save_ptr);
  //---------------------------------------//

  /* Create a new thread to execute FILE_NAME. */
  
  lock_acquire(&file_lock);
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, fn_copy2, &inode);
  else {printf("dir is null");}
  dir_close (dir);
  lock_release(&file_lock);
  
  if (inode == NULL) {
    free (fn_copy2);
    palloc_free_page (fn_copy); 
    return TID_ERROR;
  }
  tid = thread_create (fn_copy2, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy); 
  }
  free(fn_copy2);
  struct thread* t = get_thread(tid);
  sema_down(&t->process_wait_sema);
 //-----------------------------------
//  struct thread *tc = thread_current();
//  struct list_elem *e;
//  struct child_process* thiscp= NULL;
/*  
  for (e = list_begin(&tc->children_processes); e != list_end (&tc->children_processes); e = list_next(e)) {
    struct child_process *chp = list_entry (e, struct child_process, cp_elem);
    if (chp->tid == tid) {
      thiscp = chp;
    }
  }
*/
  struct child_process *chp = get_child_process(tid,&thread_current()->children_processes);
  if (chp-> exit_status == -1){
    tid = TID_ERROR;
  }
//---------------------------------
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  //------ARGPASS-------//
  char *token, *save_ptr;//pointers for tokenization
  int argc, i; //argument counts
  int *offsets; //array of offsets
  size_t file_name_len;
  void *origin; //stack pointer saved before pushing args in order
  //-------------------------------//

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  //-------------ARGPASS--------------//
  argc = 0;
  file_name_len = strlen (file_name);
  //check for stack overflow
  if (file_name_len > MAX_ARG_SIZE && 
      is_user_vaddr (if_.esp - file_name_len)) {
    printf("Arguments/File name too long\n");
    exit(RET_ERROR, NULL);
  }
  //allocate memory of array of offsets
  offsets = (int *) malloc (file_name_len * sizeof(int));
  if (offsets == NULL) {
    exit(RET_ERROR, NULL);
  }
 
  //calculate the offsets and argument count
  offsets[0] = 0;
  for (token = strtok_r (file_name, " ", &save_ptr);
       token != NULL;
       token = strtok_r (NULL, " ", &save_ptr)) {
    while (*save_ptr == ' ') save_ptr++;
    argc++;
    offsets[argc] = save_ptr - file_name; 
  }  
  //-------------------------------------//
  success = load (file_name, &if_.eip, &if_.esp);
  //-------------ARGPASS-----------------//

  if (success) {
    if_.esp -= file_name_len + 1;
    origin = if_.esp;
    memcpy (if_.esp, file_name, file_name_len + 1);

    //round down to a multiple of 4
    if_.esp -= INSTR_SIZE - ((file_name_len + 1) % INSTR_SIZE);

    //0 for arg_list[argc] and word-align
    if_.esp -= INSTR_SIZE;
    *(int *) if_.esp = 0;
   
    //push elem right to left
    for (i = argc - 1; i >= 0; i--) {
      if_.esp -= INSTR_SIZE;
      *(char **) if_.esp = origin + offsets[i];
    }
    //argv points to arg_list[0]
    if_.esp -= INSTR_SIZE;
    *(char **) if_.esp = if_.esp + INSTR_SIZE;
    //pushing argc
    if_.esp -= INSTR_SIZE;
    *(int *)if_.esp = argc;
    //return addr
    if_.esp -= INSTR_SIZE;
    *(int *)if_.esp = 0;
  
     
    //this denies write to executable file if success --------------
    lock_acquire(&file_lock);
    thread_current()-> execfile = filesys_open(file_name);
    lock_release(&file_lock);
    file_deny_write(thread_current()-> execfile);
  } 

  free (offsets);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
	 arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
  sema_up(&thread_current()-> process_wait_sema); 
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  // -------------------------------------------------------------------------------- exit lock 
  struct thread *child = get_thread(child_tid);
  struct child_process *cp 
    = get_child_process(child_tid, &thread_current()->children_processes);
  // if child's exit is called between here and the end of if statements if(child==NULL) might 
  // not be met when it should, therefor exit lock is required to prevent child from exiting
  if(cp == NULL){
    /*invalid tid*/
    /* will not race because only current thread can create new cp*/
    return RET_ERROR;
  }
  
  if(cp->waited == true){
    /*process_wait has already been called on it*/
    /* will not race because cp->wait will only be chaged here in this function
     * and wait can only be called by the parent's thread which is unique for each child*/
    return RET_ERROR;
  }
  cp->waited = true;
  if(child == NULL){
    /*child thread no longer exist, ie terminated 
      if terminated by kernel will return -1*/
    /* will not race because only the current thread can create a new child i
     * so if get_thread returns NULL it will not change */
    return cp->exit_status;
  }


  //---------------------------------------------------------------------------------- exit lock release
  //// but if it exits here???
  /*actual waiting*/
  
  sema_down(&child->process_wait_sema);
  return cp->exit_status;
}

struct child_process*
get_child_process(tid_t child_tid, struct list *children_list){
  struct list_elem* e;
  struct child_process* child;
  e = list_begin (children_list);
  while (e != list_end (children_list)) {
    child = list_entry (e, struct child_process, cp_elem);
    if(child->tid == child_tid){
      return child;
    }
    e = list_next(e);
    
  }
  return NULL; 
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  //----------Task 2-------------//
  if (!list_empty(&cur->process_wait_sema.waiters)) {
    sema_up(&cur->process_wait_sema);  
  }

  //free the child process malloc
  struct list_elem* e;
  struct child_process* child;
  e = list_begin (&cur->children_processes);
  while (e != list_end (&cur->children_processes)) {
    child = list_entry (e, struct child_process, cp_elem);
    e = list_next(e);
    free((void*)child); 
  }

  e = list_begin (&cur->files);
  while (e != list_end (&cur->files)) {
    struct file_map *fm = list_entry (e, struct file_map, elem);
    e = list_next(e);
    free(fm);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  struct supp_page_table* cur_spt = cur->supp_page_table;
  pd = cur_spt->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur_spt->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  if(cur_spt != NULL)
  {
    spt_destroy(cur_spt);
  }
    
  //allows write to executable file after exit -----
  if (cur-> execfile != NULL) {
    file_allow_write(cur-> execfile);
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->supp_page_table->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->supp_page_table = spt_init();
  if (t->supp_page_table->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  
  lock_acquire(&file_lock);
  file = filesys_open (file_name);
  lock_release(&file_lock);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = frame_get_page (upage, PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = frame_get_page (((uint8_t *) PHYS_BASE) - PGSIZE, PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->supp_page_table->pagedir, upage) == NULL
          && pagedir_set_page (t->supp_page_table->pagedir, upage, kpage, writable));
}
