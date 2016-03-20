#include <stddef.h>
#include <debug.h>
#include "threads/thread.h"

#include "vm/swap.h"


struct swap_table
{
  struct block* swap_block;
  // some ways of keeing track of the used and unused space in block
  // TODO: something like a bitmap
  // possibly a hashmap to store pid+uvaddr -> sector mapping of each page saved here
  // for now uses list
  struct list owner_mapping;
}

static struct swap_table swap_table;

struct swap_table_entry
{
  struct list_elem elem;
  uint8_t *upage;
  void* physical;
};

void
init_swap_table(void)
{
  swap_table->swap_block = block_get_role(BLOCK_SWAP); 
  // TODO:initialise the bitmap
  list_init(&swap_table->owner_mapping);
}

// saves data from frame address kpage into the swap slot
void
evict_to_swap(tid_t thread, uint8_t *upage, void* kpage)
{
  //1. block_write
  //2. update swap_table 
  //    - mark bitmap
  //    - add to hashmap
}

// looks up upage from swap slot, puts the data from swap slot to a frame and return the frame address 
void*
swap_restore_page(void* upage)
{
  owner_upage = convert(upage);
  //1. find from hashmap
  block_sector_t sector = lookup_hashmap(swap_table->owner_mapping, owner_upage);
  if(sector == NULL){
    return NULL;
  }
  void* free_frame = frame_get_page(upage, PAL_USER);
  block_read(swap_table->swap_block, sector, free_frame);
  return free_frame;
}
