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


/* See 5.1.4 : (called from pagefault handler) 
 * returns the location of the data belongs to upage that causes pagefault */
void*
supp_pt_locate_fault (struct supp_page_table* spt, uint8_t* upage)
{
  
  if(0 <= upage && upage <= PHYS_BASE){
    if(spt->evicted[upage] == 1){
      /* page is now in swap slot (provided swap slot is the only place we evict data to) */
      /* (1) find it from the slot .. how? => results in found_swap find writable also */
      /* (2): obtain a frame to store the page */
      void* free_frame = frame_get_page(upage, PAL_USER);
      /* (3) fetch data into frame */
      free_frame = found_swap;
      /* (4) point the page table entry for the faulting virtual address to frame */
      evicted[upage] = 0;
      res = pagedir_set_page (spt->pagedir, upage, free_frame, writable);
      if(!res){
        ASSERT (false); // panics, will this ever happen?
      }
    }
  } 
  // todo: check if fault causes by writing to read-only page is already covered here. (it should)
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


