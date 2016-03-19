#include "vm/page.h"
#include "vm/frame.h"
#include <stddef.h>
#include <debug.h>

// TODO: CHANGE ALL OPERATIONS WITH PAGEDIR IN PROCESS.C TO FUNCTIONS IN THIS FILE INSTEAD
// TODO: change all reference to pagedir in process.c to spt->pagedir

supp_pt_init (struct supp_page_table* spt)
{
  spt->pagedir = pagedir_create();   
  spt->evicted = {0} // initialises all elements in evicted array to zero 
}


/* See 5.1.4 point 1 : (called from pagefault handler) 
 * returns the location of the data belongs to upage that causes pagefault */
void*
supp_pt_locate_fault (struct supp_page_table* spt, uint8_t* upage)
{
  
  if(0 <= upage && upage <= PHYS_BASE){
    if(spt->exists[evicted] == 1){
      /* page is now in swap slot (provided swap slot is the only place we evict data to)
       * 1. find it from the slot .. how?
       * 2. do the necessary swaps and updates?
       * 3. return the address of the kernel virtual address?
       */


    }
  } 
  process_exit();
}


/* Destroys page directory PD, freeing all the pages it
   references, 
   and free other pages belonged to the process that are not in the pagedir */
void
supp_pt_destroy (struct supp_page_table* spt) {
  pagedir_destroy (spt->pagedir);
  // TODO: free other pages that is marked true in evicted array 
}


