#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <stddef.h>
#include <debug.h>
///// include 2 hash libs

// TODO: CHANGE ALL OPERATIONS WITH PAGEDIR IN PROCESS.C TO FUNCTIONS IN THIS FILE INSTEAD
// TODO: change all reference to pagedir in process.c to spt->pagedir
//

static supp_page_table supp_page_table;

void
init_page(struct page* p, tid_t owner_tid, void* raw_upage){
  p->tid = owner_tid;
  p->upage = pg_round_down(raw_upage);
}

//---------------------------------------------------------------------- hash table functions ------- //

struct page_table_entry
{
  struct hash_elem hash_elem;
  struct page* page;
};

struct page_table_entry*
pte_malloc_init(struct page* p)
{
  struct page_table_entry* entry = malloc(sizeof(struct page_table_entry));
  entry->page = p; 
  return entry
}
unsigned 
page_table_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page_table_entry *p = hash_entry (p_, struct swap_table_entry, hash_elem);
  return hash_bytes (&p->page->tid, sizeof p->page->tid) ^ hash_bytes (&p->page->upage, sizeof p->page->upage);
}

/* less function for swap hash table. See spec A.8.5 */
bool
page_table_less (const struct hash_elem *a_, const struct hash_elem *b_,
                  void *aux UNUSED)
{
  const struct swap_table_entry *a = hash_entry (a_, struct page_table_entry, hash_elem);
  const struct swap_table_entry *b = hash_entry (b_, struct page_table_entry, hash_elem);

  if(a->page->tid != b->page->tid){
    return a->page->tid < b->page->tid;
  }
  return a->page->upage < b->page->upage;
}

struct page_table_entry*
page_table_lookup (const struct page p)
{
  struct swap_table_entry dummy;
  struct hash_elem *e;

  dummy.page = p;
  e = hash_find (&supp_page_table->page_table, &dummy.hash_elem);
  return e != NULL ? hash_entry (e, struct page_table_entry, hash_elem): NULL;
}

struct page_table_entry*
page_table_remove (const struct page p)
{
  struct swap_table_entry dummy;
  struct hash_elem *e;

  dummy.page = p;
  e = hash_delete (&supp_page_table->page_table, &dummy.hash_elem);
  return e != NULL ? hash_entry (e, struct page_table_entry, hash_elem): NULL;
}
//------------------------------------------------------------------ end hash table functions ------- //

//TODO: put this into init.c
void
supp_pt_init (struct supp_page_table* spt)
{
  spt->pagedir = pagedir_create();   
  hash_init(&supp_page_table->page_table, page_table_hash, page_table_less, NULL);
}

void*
page_table_get_kpage (void* raw_upage)
{
  void* upage = pg_round_down (raw_upage);
  tid_t tid = thread_current()->tid;
  struct page dummy;
  init_page(&dummy, tid, upage);
  struct page_table_entry* found_pte = page_table_lookup(dummy);
  if(found_pte != NULL)
  {
    return found_pte->page->kpage;  //TODO: do i need to put offset back to kpage
  }
  else{
    //can't find in supp page table => try to find in swap
    return supp_pt_locate_fault (void* upage);
  }
}

/* For frame.c to use when eviction happens. Given tid and upage, remove page table entry */
bool
supp_page_table_remove (tid_t tid, void* raw_upage)
{
  struct page dummy;
  init_page(&dummy, tid, upage);
  struct page_table_entry* removed = page_table_remove(dummy);
  if(removed == NULL){
    //can't find entry/ nothing is removed
    return false;
  }
  return true;
}

/* For frame.c to use when getting new page. Given tid and upage, add entry to page table */
bool
supp_page_table_add (tid_t tid, void* raw_upage)
{
  void* upage = pg_round_down (raw_upage);
  struct page* p = malloc(sizeof(strict page));
  init_page(p, tid, upage);
  struct page_table_entry* pte = pte_malloc_init(p);
  struct page_table_entry* added = hash_insert(&supp_page_table->page_table, &pte->hash_elem);
  if(added == NULL){
    // nothing is added
    return false;
  }
  return true;
}
//--------------------------------//


/* See 5.1.4 : (called from pagefault handler) 
 * returns the location of the data belongs to upage that causes pagefault */
// TODO: please return null when spt->evicted[upage/PGSIZE] == 0
void*
supp_pt_locate_fault (struct supp_page_table* spt, uint8_t* upage)
{
  
  if(is_user_vaddr(upage)){
    if(spt->evicted[upage/PGSIZE] == 1){
      /* page is now in swap slot (provided swap slot is the only place we evict data to) */
      /* (1) find it from the slot */
      /* (2): obtain a frame to store the page */
      /* (3) fetch data into frame */
      void* kpage = swap_restore_page(upage);
      /* (4) point the page table entry for the faulting virtual address to frame */
      evicted[upage/PGSIZE] = 0;
      res = pagedir_set_page (spt->pagedir, upage, free_frame, writable);
      if(!res){
        ASSERT (false); // panics, will this ever happen?
      }
      return kpage;
    }
  } 
  // todo: check if fault causes by writing to read-only page is already covered here. (it should)
  process_exit();
}



