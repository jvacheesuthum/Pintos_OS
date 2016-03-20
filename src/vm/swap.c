#include <stddef.h>
#include <debug.h>
#include "threads/thread.h"

#include "vm/swap.h"


struct swap_table
{
  struct block* swap_block;
  // some ways of keeing track of the used and unused space in block
  // TODO: something like a bitmap
  struct hash swap_hash_table;
  struct list owner_mapping;
}

static struct swap_table swap_table;

//---------------------------------------------------------------------- hash table functions ------- //

struct swap_table_entry
{
  struct hash_elem hash_elem;
  tid_t tid;
  void* upage; 
  block_sector_t swap_sector;
};

struct swap_table_entry*
ste_init(tid_t tid, void* raw_upage)
{
  struct swap_table_entry* entry = malloc(sizeof swap_table_entry);
  res->upage = pg_round_down (raw_upage);
  return entry; 
}

/* hash function for swap hash table. See spec A.8.5*/
unsigned 
swap_table_hash (const struct hash_elem *ste_, void *aux UNUSED)
{
  const struct swap_table_entry *ste = hash_entry (ste_, struct swap_table_entry, hash_elem);
  return hash_bytes (&ste->tid, sizeof ste->tid) ^ hash_bytes (&ste->upage, sizeof ste->upage);
}

/* less function for swap hash table. See spec A.8.5 */
bool
swap_table_less (const struct hash_elem *a_, const struct hash_elem *b_,
                  void *aux UNUSED)
{
  const struct swap_table_entry *a = hash_entry (a_, struct swap_table_entry, hash_elem);
  const struct swap_table_entry *b = hash_entry (b_, struct swap_table_entry, hash_elem);

  if(a->tid != b->tid){
    return a->tid < b->tid;
  }
  return a->upage < b->upage;
}

struct swap_table_entry*
swap_hash_table_lookup (const tid_t tid, const void *raw_upage)
{
  struct swap_table_entry dummy;
  struct hash_elem *e;

  dummy.tid = tid;
  dummy.upage = pg_round_down(raw_upage);
  e = hash_find (&swap_table->swap_hash_table, &dummy.hash_elem);
  return e != NULL ? hash_entry (e, struct swap_table_entry, hash_elem): NULL;
}

//------------------------------------------------------------------ end hash table functions ------- //

void
init_swap_table(void)
{
  swap_table->swap_block = block_get_role(BLOCK_SWAP); 
  // TODO:initialise the bitmap
  hash_init(&swap_table->swap_hash_table, swap_table_hash, swap_table_less, NULL);
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
  // TODO: should be (void* upage, tid)
void*
swap_restore_page(void* raw_upage)
{
  owner_upage = convert(upage);
  struct swap_table_entry* found_swe = swap_hash_table_lookup(thread_current()->tid, raw_upage);
  if(found_swe == NULL) { return NULL; }
  block_sector_t sector = found_swe->swap_sector;

  void* free_frame = frame_get_page(upage, PAL_USER);
  block_read(swap_table->swap_block, sector, free_frame);
  return free_frame;
}
