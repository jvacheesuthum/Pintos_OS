#include <stddef.h>
#include <debug.h>
#include "threads/thread.h"

#include "vm/swap.h"


struct swap_table
{
  struct block* swap_block;
  block_sector_t new_swap_slot;     /* keeps track of the next free swap slots incremented by PGSIZE/BLOCK_SECTOR_SIZE
                            everytime a page is put into new slot of swap_block */
  struct list free_slots; /* looks in here to see free slot first, if empty then request new slot */
  struct hash swap_hash_table;
}

struct free_slot
{
  struct list_elem elem;
  block_sector_t slot_begin;
}

static struct swap_table swap_table;

//---------------------------------------------------------------------- hash table functions ------- //

struct swap_table_entry
{
  struct hash_elem hash_elem;
  tid_t tid;
  void* upage; 
  block_sector_t swap_slot;
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
swap_hash_table_remove (const tid_t tid, const void *raw_upage)
{
  struct swap_table_entry dummy;
  struct hash_elem *e;

  dummy.tid = tid;
  dummy.upage = pg_round_down(raw_upage);
  e = hash_delete (&swap_table->swap_hash_table, &dummy.hash_elem);
  return e != NULL ? hash_entry (e, struct swap_table_entry, hash_elem): NULL;
}

//------------------------------------------------------------------ end hash table functions ------- //

void
init_swap_table(void)
{
  swap_table->swap_block = block_get_role(BLOCK_SWAP); 
  swap_table->new_swap_slot = 0; 
  list_init(&swap_table->free_slots);
  hash_init(&swap_table->swap_hash_table, swap_table_hash, swap_table_less, NULL);
}

// write into swap block and insert new entry into swap_hash_table
void
evict_to_swap(tid_t tid, void *raw_upage, void* kpage)
{
  block_sector_t free_slot = get_free_slot();
  swap_write(free_slot, kpage);

  struct swap_table_entry* ste = ste_init(thread, raw_upage);
  ste->swap_slot = free_slot;
  struct hash_elem* old = hash_insert(&swap_table->swap_hash_table, &ste->hash_elem);
  if(old != NULL){
    ASSERT (false); //probably indicates bugs
  }
}

void*
swap_restore_page(void* raw_upage)
{
  struct swap_table_entry* found_swe = swap_hash_table_remove(thread_current()->tid, raw_upage);
  if(found_swe == NULL) { return NULL; }
  block_sector_t slot = found_swe->swap_slot;
  free(found_swe);

  void* found_frame = frame_get_page(upage, PAL_USER);
  swap_read(slot, found_frame);

  struct free_slot* fs = malloc(sizeof(struct free_slot));
  fs->slot_begin = slot;
  list_push_back (&swap_table->free_slot, &fs->elem);

  return found_frame;
}

block_sector_t
get_free_slot(void)
{
  if(!list_empty(&swap_table->free_slots)){
    struct free_slot* fs = &list_entry (list_pop_front(&swap_table->free_slots));
    block_sector_t res = fs->slot_begin;
    free(fs);
    return res;
  } else {
    block_sector_t res = swap_table->new_swap_slot;
    swap_table->new_swap_slot += PGSIZE/BLOCK_SECTOR_SIZE;
    return res;
  }
}

void 
swap_read(block_sector_t slot_begin_sector, void* buffer)
{
  // TODO: read for 8 sectors
  block_read(swap_table->swap_block, sector, free_frame);
}

void 
swap_write(block_sector_t slot_begin_sector, void* buffer)
{
  //TODO: block_write for 8 sectors
}
