#include "vm/frame.h"
#include "list.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"
#include <stddef.h>
#include <debug.h>
#include <stdio.h>

void frame_free_page(tid_t thread);
void frame_pin_page(tid_t thread, uint8_t* upage);
void* evict(uint8_t *newpage);

// should actually use hash but for testing correctness use list for now.
struct frame
  {
    struct list_elem elem;
    tid_t thread;
    uint8_t *upage;
    void* physical;
    bool pinned;
  };

static struct list frame_table;

void 
frame_table_init(void) 
{
  list_init(&frame_table);
}

// eviction according to second chance
void*
evict(uint8_t *newpage)
{ 
//  printf("started eviction\n"); 
  // cannot evict empty list (and should not)
  ASSERT(!list_empty(&frame_table));

  struct list_elem* e = list_begin(&frame_table);
  struct frame* toevict = list_entry(e, struct frame, elem);
  uint32_t *pd = (get_thread(toevict->thread))->pagedir; 
  while (toevict->pinned || pagedir_is_accessed(pd, toevict->upage))
  {
    e = list_remove(e);
    pagedir_set_accessed(pd, toevict->upage, false);
    list_push_back(&frame_table, &toevict->elem);
    toevict = list_entry(e, struct frame, elem);
    pd = (get_thread(toevict->thread))->pagedir; 
  }
  // found unaccessed page, evict
  list_remove(e);
  // remove pagedir entry for evicted page
  pagedir_clear_page(pd, toevict->upage);
  // mark sup pt evicted.
  spt_mark_evicted(toevict->thread, toevict->upage);
  // place contenets into swap. should need to save the thread and upage too.
   evict_to_swap(toevict->thread, toevict->upage, toevict->physical);
// change frame to newpage
  toevict->thread = thread_current()->tid;
  toevict->upage = newpage;
  list_push_back(&frame_table, e);

  return toevict->physical;
//  printf("eviction complete\n");
}

// only for user processes. 
//kernel should directly use palloc get page as original
void* 
frame_get_page(void* raw_upage, enum palloc_flags flags)
{
  ASSERT (flags & PAL_USER)
  void* upage = pg_round_down (raw_upage); 

  void* result = palloc_get_page(flags);
 
  //updating page tables and per_process_upages list
  /*
  struct per_process_upages_elem* el = malloc(sizeof(struct per_process_upages_elem));
  el->upage = upage;
  list_push_back(&thread_current()->per_process_upages_elem, &el->elem);
  supp_table_add(thread_current()->tid, upage);
  */
//if there is no empty frames, try to evict
  if (result == NULL) {
    void* result = evict(upage);
  // if eviction was not successfull, panic
    ASSERT (result != NULL);
  // no need to add entry.
    return result;
  }

//add entry to frame table
  struct frame* entry = malloc(sizeof(struct frame));
  entry->thread = thread_current()->tid;
  entry->upage = upage;
  entry->physical = result;
  entry->pinned = false;
  list_push_back(&frame_table, &entry->elem);
//  printf("thread %d putting upage %d to kpage %d\n", entry->thread, upage, result);

  return result;

}

void 
frame_free_page(tid_t thread)
{
  struct list_elem* e = list_begin(&frame_table);
  while (e != list_end(&frame_table))
  {
    struct frame* inspecting = list_entry(e, struct frame, elem);
    if (thread == inspecting->thread){
      e = list_remove(e);
      free(inspecting);
    }
    else e = list_next(e);
  }
}

void frame_pin_page(tid_t thread, uint8_t* upage)
{
  struct frame* topin = NULL;
  struct list_elem* e = list_end(&frame_table);
  while (e != list_begin(&frame_table) && topin != NULL)
  {
    struct frame* inspecting = list_entry(e, struct frame, elem);
    if (thread == inspecting->thread && upage == inspecting->upage){
      topin = inspecting;
    }
    e = list_prev(e);
  }

  if (topin != NULL) {
    topin->pinned = true;
  }
  else ASSERT (false);
}
