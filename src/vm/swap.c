#include <stddef.h>
#include <debug.h>
#include "threads/thread.h"
#include "devices/block.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "list.h"
#include "devices/block.h"
#include "threads/synch.h"
#include <stdio.h>

struct swap_table
{
  struct block* swap_block;
  block_sector_t new_swap_slot;     /* keeps track of the next free swap slots incremented by PGSIZE/BLOCK_SECTOR_SIZE
                            everytime a page is put into new slot of swap_block */
  struct list free_slots; /* looks in here to see free slot first, if empty then request new slot */
  struct hash swap_hash_table;
  struct lock swap_lock;
};

struct free_slot
{
  struct list_elem elem;
  block_sector_t slot_begin;
};

static struct swap_table swap_table;

//prototypes
struct swap_table_entry* ste_malloc_init(tid_t tid, void* raw_upage);
unsigned swap_table_hash (const struct hash_elem *ste_, void *aux UNUSED);
bool swap_table_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct swap_table_entry* swap_hash_table_remove (const tid_t tid, const void *raw_upage);

block_sector_t get_free_slot(void);
void swap_read(block_sector_t slot_begin_sector, void* buffer);
void swap_write(block_sector_t slot_begin_sector, void* buffer);


//---------------------------------------------------------------------- hash table functions ------- //

struct swap_table_entry
{
  struct hash_elem hash_elem;
  tid_t tid;
  void* upage; 
  block_sector_t swap_slot;
};

struct swap_table_entry*
ste_malloc_init(tid_t tid, void* raw_upage)
{
  struct swap_table_entry* entry = malloc(sizeof(struct swap_table_entry));
  entry->tid = tid;
  entry->upage = pg_round_down (raw_upage);
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
  e = hash_delete (&swap_table.swap_hash_table, &dummy.hash_elem);
  return e != NULL ? hash_entry (e, struct swap_table_entry, hash_elem): NULL;
}

//------------------------------------------------------------------ end hash table functions ------- //

void
init_swap_table(void)
{
  swap_table.swap_block = block_get_role(BLOCK_SWAP); 
  swap_table.new_swap_slot = 0; 
  list_init(&swap_table.free_slots);
  hash_init(&swap_table.swap_hash_table, swap_table_hash, swap_table_less, NULL);
  lock_init(&swap_table.swap_lock);
}

// write into swap block and insert new entry into swap_hash_table
void
evict_to_swap(tid_t tid, void *raw_upage, void* kpage)
{
  lock_acquire(&swap_table.swap_lock);
  block_sector_t free_slot = get_free_slot();
  //printf("got a free slot, writing from kpage %d\n", kpage);
  swap_write(free_slot, kpage);
  //printf("swap wrote\n");
  struct swap_table_entry* ste = ste_malloc_init(tid, raw_upage);
  ste->swap_slot = free_slot;
  struct hash_elem* old = hash_insert(&swap_table.swap_hash_table, &ste->hash_elem);
  //printf("size of swap table is %d\n",hash_size(&swap_table.swap_hash_table));
  lock_release(&swap_table.swap_lock);
  if(old != NULL){
    ASSERT (false); //probably indicates bugs
  }
}

void*
swap_restore_page(tid_t tid, void* raw_upage)
{
//  lock_acquire(&swap_table.swap_lock);
  void* upage = pg_round_down (raw_upage);
  struct swap_table_entry* found_swe = swap_hash_table_remove(tid, raw_upage);
  if(found_swe == NULL) {
  //`  printf("always null!");
     return NULL; }
  printf("not null");
  block_sector_t slot = found_swe->swap_slot;
  free(found_swe);

  void* found_frame = frame_get_page(upage, PAL_USER);
  swap_read(slot, found_frame);

  struct free_slot* fs = malloc(sizeof(struct free_slot));
  fs->slot_begin = slot;
  list_push_back (&swap_table.free_slots, &fs->elem);

  //set evicted back to 0
  spt_unmark_evicted(tid, upage);
//  lock_release(&swap_table.swap_lock);
  return found_frame;
}

// remove from hash table and add to free slot list
void
swap_clear_slot(void* raw_upage)
{
  lock_acquire(&swap_table.swap_lock);
  void* upage = pg_round_down (raw_upage);
  struct swap_table_entry* found_swe = swap_hash_table_remove(thread_current()->tid, raw_upage);
  ASSERT(found_swe != NULL);
  block_sector_t slot = found_swe->swap_slot;
  free(found_swe);
  
  struct free_slot* fs = malloc(sizeof(struct free_slot));
  fs->slot_begin = slot;
  list_push_back (&swap_table.free_slots, &fs->elem);
  lock_release(&swap_table.swap_lock);
}

block_sector_t
get_free_slot(void)
{
  lock_acquire(&swap_table.swap_lock);
  if(!list_empty(&swap_table.free_slots)){
    struct free_slot* fs = list_entry (list_pop_front(&swap_table.free_slots), struct free_slot, elem);
    block_sector_t res = fs->slot_begin;
    free(fs);
    lock_release(&swap_table.swap_lock);
    return res;
  } else {
    block_sector_t res = swap_table.new_swap_slot;
    swap_table.new_swap_slot += PGSIZE/BLOCK_SECTOR_SIZE;
    lock_release(&swap_table.swap_lock);
    return res;
  }
}

// reads kpage size of data (8 sectors) from begin to kpage
void 
swap_read(block_sector_t begin, void* kpage)
{
  int i;
  for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++) {
    block_read(block_get_role(BLOCK_SWAP), begin + i, kpage + BLOCK_SECTOR_SIZE*i);
  }
}

//writes kpage size of data (8 sectors) from kpage to begin
void 
swap_write(block_sector_t begin, void* kpage)
{
  int i;
  for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++) {
    //printf("i is %d\n", i);
    //printf("begin is %d\n", begin +i);
    //printf("page is %d\n", kpage + BLOCK_SECTOR_SIZE*i);
    block_write(block_get_role(BLOCK_SWAP), begin + i, kpage + BLOCK_SECTOR_SIZE*i);
  }
}

/*
void 
per_process_cleanup_swap()
{
  struct supp_page_table* spt = thread_current()->supp_page_table;
  int i;
  for(i = 0; (uint32_t) i < PG_TOTAL; i++){
    if(spt->evicted[i] == 1){
      //remove entry from hash table
      void* upage = ((uint32_t)i)*PG_SIZE;
      struct swap_table_entry* found_swe = swap_hash_table_remove(thread_current()->tid, upage);
      block_sector_t slot = found_swe->swap_slot;
      free(found_swe);
      //add to free slots list
      struct free_slot* fs = malloc(sizeof(struct free_slot));
      fs->slot_begin = slot;
      list_push_back (&swap_table.free_slots, &fs->elem);
    }
  }
}
*/
