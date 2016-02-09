#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/kernel/list.h"
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif


static struct list sleep_list;
/* to_update_list keeps tracks of the threads that recieved recent_cpu 
 * incrementation, to be used and cleared after 4th tick priority update */
static struct list to_update_list;
/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);
static bool list_contains( struct list* l, struct list_elem* e);

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  list_init (&sleep_list);
  list_init (&to_update_list);
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  ASSERT (intr_get_level () == INTR_ON);

  /*init sleeping thread*/
  struct sleeping_thread st;
  list_init(&(st.sema.waiters));
  sema_init(&(st.sema), 0);
  st.alarm_ticks = ticks;
  list_push_back (&sleep_list, &(st.elem));

  ASSERT (intr_get_level () == INTR_ON);
  
  sema_down(&(st.sema));
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
/* uses thread_yield_on_return at thread_tick () which causes the preempted thread to be yield after the interrupt */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  enum intr_level old_level;
  ticks++;
  thread_tick ();

  

  /*alarm - when timer is sleeping*/
  struct list_elem* e;
  e = list_begin (&sleep_list);
  while (e != list_end (&sleep_list)) {
    struct sleeping_thread *st = list_entry (e, struct sleeping_thread, elem);
    st->alarm_ticks--;
    if (st->alarm_ticks <= 0) {
      sema_up (&(st->sema));
      e = list_remove(e);
    } else {
      e = list_next(e);
    }
  }
  
  /*TASK1 mlfqs: updating recent_cpu values*/
  if(thread_mlfqs){
    bool forth_tick = timer_ticks() % 4 == 0;
    bool sec_tick = timer_ticks() % 100 == 0;
    struct thread* cur = thread_current();
    
    /*increment recent_cpu*/
    cur -> recent_cpu += FP_CONV;
    cur -> needs_update = true;

/*
    struct list_elem* upelem = &cur->update_elem;
    if(upelem->prev == NULL){ //checks if cur->update_elem not interior, same as !is_interior
      list_push_back(&to_update_list, upelem);
    }

    old_level = intr_disable();
    if(forth_tick && (!sec_tick)){
      int el_size = list_size(&to_update_list);
      struct list_elem* el;
      update_priority_of(cur, NULL);
      while (el_size > 0){
        el = list_pop_back(&to_update_list);
        update_priority_of(list_entry(el, struct thread, update_elem), NULL);
        el_size--;
      }
    }
    if(sec_tick){
      update_load_avg();
      thread_foreach(&update_recent_cpu_of, NULL);
      thread_foreach(&update_priority_of, NULL);
      
      int el_size = list_size(&to_update_list);
      struct list_elem* el;
      while (el_size > 0){
        el = list_pop_back(&to_update_list);
        el_size--;
      }
    }
  intr_set_level(old_level);
*/

    old_level = intr_disable();
    if(sec_tick){
      update_load_avg();
      thread_foreach(&update_recent_cpu_of, NULL);
    }
    if(forth_tick){
      thread_foreach(&update_priority_of, NULL);
    }
    intr_set_level(old_level);
  }
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

static bool
list_contains(struct list* l, struct list_elem* el){
  struct list_elem* e;
  e = list_begin (l);
  while (e != list_end (l)) {
    if(e == el){
      return true;
    }
    e = list_next(e);
  }
  return false;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
