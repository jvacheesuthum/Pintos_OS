#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include <stddef.h>
#include <debug.h>
#include <hash.h>
#include "lib/kernel/hash.h"

//prototypes
unsigned tid_hash (const struct hash_elem *p_, void *aux UNUSED);
bool tid_less (const struct hash_elem *a_, const struct hash_elem *b_,void *aux UNUSED);
struct spt_entry* spt_tid_lookup (const tid_t tid);
unsigned ev_hash (const struct hash_elem *p_, void *aux UNUSED);
bool ev_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct ev_entry* ev_remove (void* upage, struct hash* ev);
struct hash* new_evicted_table(tid_t tid);

// -------------------------------------------------------------------- supp page table hash table -- //
static struct hash supp_page_table;

struct spt_entry
{
  struct hash_elem hash_elem;
  tid_t tid;
  struct hash* evicted;
};

unsigned 
tid_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spt_entry *p = hash_entry (p_, struct spt_entry, hash_elem);
  return hash_bytes (&p->tid, sizeof p->tid);
}

bool
tid_less (const struct hash_elem *a_, const struct hash_elem *b_,
                  void *aux UNUSED)
{
  const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
  const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);
  return a->tid < b->tid;
}

struct spt_entry*
spt_tid_lookup (const tid_t tid)
{
  struct spt_entry p;
  struct hash_elem *e;
  p.tid = tid;
  e = hash_find (&supp_page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct spt_entry, hash_elem) : NULL;
}

// -------------------------------- evicted hash table inside spt_entry --- //
struct ev_entry
{
  struct hash_elem hash_elem;
  void* upage;  // alreay rounded down
};

unsigned 
ev_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct ev_entry *p = hash_entry (p_, struct ev_entry, hash_elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

bool
ev_less (const struct hash_elem *a_, const struct hash_elem *b_,
                  void *aux UNUSED)
{
  const struct ev_entry *a = hash_entry (a_, struct ev_entry, hash_elem);
  const struct ev_entry *b = hash_entry (b_, struct ev_entry, hash_elem);
  return a->upage < b->upage;
}

struct ev_entry*
ev_remove (void* upage, struct hash* ev)
{
  struct ev_entry p;
  struct hash_elem *e;
  p.upage = upage;
  e = hash_delete (ev, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct ev_entry, hash_elem) :NULL;
};

struct hash* 
new_evicted_table(tid_t tid)
{
  struct hash* h = malloc(sizeof (struct hash)); //TODO:include malloc
  hash_init (h, ev_hash, ev_less, NULL);
  return h;
}

// -------------------------------------------------------------- end ----- //

// ------------------------------------------------------------------------------------------- end -- // 


void
supp_page_table_init()
{
  hash_init (&supp_page_table, tid_hash, tid_less, NULL);
}

void
spt_mark_evicted(tid_t tid, void* upage)
{
  ASSERT(pg_ofs(upage) == 0);
  struct spt_entry* found_spt_e = spt_tid_lookup(tid);
  struct hash* found_evicted;
  if(found_spt_e == NULL){
    found_evicted = new_evicted_table (tid);
  } else{
    found_evicted = found_spt_e->evicted;
  }
  struct ev_entry* new_ev_e = malloc(sizeof (struct ev_entry));
  new_ev_e->upage = upage;
  struct hash_elem* res = hash_insert(found_evicted, &new_ev_e->hash_elem);
  ASSERT(res == NULL); // res!=null means entry already in hash which indicates bug 
}

void 
spt_unmark_evicted(tid_t tid, void* upage)
{ 
  ASSERT(pg_ofs(upage) == 0);
  struct spt_entry* found_spt_e = spt_tid_lookup(tid);
  if(found_spt_e == NULL){
    ASSERT(false); // can't find in hash, indicates bug
  }
  struct hash* found_evicted = found_spt_e->evicted;
  struct ev_entry* found_ev_e = ev_remove(upage, found_evicted);
  free(found_ev_e);
  if(hash_empty(found_evicted)){
    free(found_evicted);
  }
}

void 
cleanup_evicted()
{
  //1. find the tid hash
  struct spt_entry* found_spt_e = spt_tid_lookup(thread_current()->tid);
  ASSERT(found_spt_e != NULL);
  struct hash* found_evicted = found_spt_e->evicted;
  //2. iterate through it
  struct hash_iterator i;
  hash_first(&i,found_evicted);
  while(hash_next(&i))
  {
    //TODO: does each ev_entry have to be freed? // but can't alter hash in iterator
  //3. find entry in swap table, call remove slot function
    struct ev_entry* each_ev = hash_entry(hash_cur(&i), struct ev_entry, hash_elem);
    swap_clear_slot(each_ev->upage); 
  }
}

/* See 5.1.4 :  
 * returns the location of the data belongs to upage that causes pagefault */
void*
supp_pt_locate_fault (void* upage)
{
  
  if(is_user_vaddr(upage)){
    /* page is now in swap slot (provided swap slot is the only place we evict data to) */
    /* (1) find it from the slot : done in swap_restore_page => swap_hash_table_remove*/
    /* (2): obtain a frame to store the page: done in swap_restore_page => frame_get_page*/
    /* (3) fetch data into frame : done in swap_restore_page */
    /* (4) point the page table entry for the faulting virtual address to frame : done in swap_restore_page => frame_get_page*/
    void* kpage = swap_restore_page(upage);
    return kpage;
  } 
  // TODO: check if fault causes by writing to read-only page is already covered here. (it should)
  process_exit();
  return NULL;
}

