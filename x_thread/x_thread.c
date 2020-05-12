#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* malloc */
#include <ucontext.h>		/* ucontext_t */
#include <sys/time.h>		/* itimerval */
#include <signal.h>		/* sigaction */
#include <sys/ioctl.h>		/* ioctl */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>		/* fcntl */
#include <fcntl.h>		/* fcntl */
#include <stropts.h>		/* SIGNAL */
#include <errno.h>		/* errno */
#include "x_thread.h"
#include "../queue/queue.h"

#define STACK_SIZE 8192
#define STATE_NUM 5
enum state {
  RUNNING,
  READY,
  ZOMBIE,
  BLOCKED,
  SLEEPING
};

struct queue *schedule[STATE_NUM];

void x_thread_swap (struct queue *running, enum state before_state, enum state after_state);
void x_thread_move (struct queue *moved, enum state before_state, enum state after_state);
struct queue *x_thread_find (int tid, enum state state);
struct queue *x_thread_find_from_head (int tid, struct queue *head);
struct queue *x_thread_find_from_all_state (int tid);
struct timeval *x_thread_awaketime (int sec, int msec);
void wakeup (void);
struct queue *x_thread_extract_shortest_time_sleeper (void);
void x_thread_check_sleep (enum state after_state);
void x_thread_fd_set (int *max_fd, fd_set *rfd_set, fd_set *wfd_set);
void x_thread_io_ready (fd_set *rfd_set, fd_set *wfd_set);

/* Call func (arg), and x_thread_exit (n).
 * This function is registered to new Thread in x_thread_create ()
 */
void
func_call_x_thread_exit (int (*func)(void *arg), void *arg)
{
  int n = func (arg);
  x_thread_exit (n);
}

/* SIGALRM handler */
void
alarm_handler (int signo)
{
  wakeup ();
  x_thread_yield ();
}

/* SIGPOLL handler */
void
sigpoll_handler (int signo)
{
  int max_fd, ready_num;
  fd_set rfd_set, wfd_set;
  struct timeval interval;
  interval.tv_sec  = 0;
  interval.tv_usec = 100;
  x_thread_fd_set (&max_fd, &rfd_set, &wfd_set);
  if (max_fd == -1)
    return;
  ready_num = select (max_fd+1, &rfd_set, &wfd_set, NULL, &interval);
  if (ready_num == 0)
    return;
  x_thread_io_ready (&rfd_set, &wfd_set); 
}

/* set SIGALRM signal handler */
void
x_thread_setitimer (void)
{
  struct sigaction act;
  struct itimerval timer;

  act.sa_handler = alarm_handler;
  act.sa_flags = 0;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGINT);
  sigaction (SIGALRM, &act, NULL);

  timer.it_value.tv_sec  = 0;
  timer.it_value.tv_usec = 100;
  timer.it_interval = timer.it_value;
  setitimer (ITIMER_REAL, &timer, NULL);
}

/* set SIGPOLL signal handler */
void
x_thread_set_sigpoll (void)
{
  int val;
  struct sigaction act;
  act.sa_handler = sigpoll_handler;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGINT);
  act.sa_flags = 0;
  sigaction (SIGPOLL, &act, NULL);

  ioctl (STDIN_FILENO, I_SETSIG, S_INPUT);

  val = fcntl (STDIN_FILENO, F_GETFL);
  fcntl (STDIN_FILENO, F_SETFL, (val | O_NONBLOCK | O_ASYNC));
  fcntl (STDIN_FILENO, F_SETOWN, getpid ());
}

/* initialize struct Thread */
void
init_Thread (struct Thread *tp, ucontext_t uc, int exit_status, struct Thread *prev, struct Thread *next)
{
  static int tid = 0;
  tp->tid = tid++;
  tp->uc = uc;
  tp->exit_status = exit_status;
  tp->join_tid = -1;
  tp->awake_time = NULL;
  tp->wait_rfd = -1;
  tp->wait_wfd = -1;
}

/* initialize x_thread system.
 * create main_thread and push it to schedule [RUNNING].
 * register signal handlers.
 */
void
x_thread_init (void)
{
  struct Thread *main_thread;
  ucontext_t uc;
  main_thread = (struct Thread *) malloc (sizeof (struct Thread));
  getcontext (&uc);
  init_Thread (main_thread, uc, 0, NULL, NULL);
  queue_init (&schedule [RUNNING], "main", 5, main_thread);
  schedule [READY]    = NULL;
  schedule [ZOMBIE]   = NULL;
  schedule [BLOCKED]  = NULL;
  schedule [SLEEPING] = NULL;

  x_thread_setitimer ();
  x_thread_set_sigpoll ();
}


/* create new Thread and push it to schedule [READY] */
int
x_thread_create (int (*func)(void *arg), void *arg)
{
  struct Thread *new_thread;
  ucontext_t uc;
  void *stack;
  new_thread = (struct Thread *) malloc (sizeof (struct Thread));
  
  stack = (void *) malloc (STACK_SIZE);
  // stack += STACK_SIZE;
  getcontext (&uc);
  uc.uc_link = NULL;
  uc.uc_stack.ss_sp = stack;
  uc.uc_stack.ss_size = STACK_SIZE;
  makecontext (&uc, func_call_x_thread_exit, 3, func, arg);

  init_Thread (new_thread, uc, 0, NULL, NULL);
  char name[10];
  snprintf (name, 10, "tid%d", new_thread->tid);
  queue_push_new (&schedule [READY], name, sizeof (name), new_thread);
  return new_thread->tid;
}

/* find shortest-time sleeper from SLEEPING */
struct queue *
x_thread_find_shortest_time_sleeper (void)
{
  if (schedule [SLEEPING] == NULL)
    return NULL;

  struct queue *scanner, *shortest_time_sleeper;
  struct timeval *shortest_time;
  shortest_time_sleeper = schedule [SLEEPING];
  shortest_time = ((struct Thread *)shortest_time_sleeper->content)->awake_time;
  for (scanner = schedule [SLEEPING]; scanner != NULL; scanner = scanner->next) {
    struct timeval *scanner_time = ((struct Thread *)scanner->content)->awake_time;
    if (scanner_time == NULL)
      continue;
    if (timercmp (scanner_time, shortest_time, <)) {
      shortest_time_sleeper = scanner;
      shortest_time = scanner_time;
    }
  }
  return shortest_time_sleeper;
}

/* extract shortest-time sleeper from SLEEPING */
struct queue *
x_thread_extract_shortest_time_sleeper (void)
{
  struct queue *shortest_time_sleeper = x_thread_find_shortest_time_sleeper ();
  if (shortest_time_sleeper == NULL)
    return NULL;
  queue_extract (&schedule [SLEEPING], shortest_time_sleeper);
  return shortest_time_sleeper;
}

/* call usleep to sleep right now.
 * This function called x_thread_check_sleep (), x_thread_self_sleep ().
 */
void
x_thread_sleep_now (struct timeval *awake_time)
{
  sigset_t old_set, new_set;
  sigemptyset (&new_set);
  sigaddset (&new_set, SIGALRM);
  sigaddset (&new_set, SIGPOLL);
  sigprocmask (SIG_SETMASK, &new_set, &old_set);

  struct timeval now, sleep_time;
  gettimeofday (&now, NULL);
  if (timercmp (&now, awake_time, <)) {
    timersub (awake_time, &now, &sleep_time);
    // printf ("#%d: %ld sec and %ld msec sleep .zZ\n", x_thread_self (), sleep_time.tv_sec, sleep_time.tv_usec);
    // fflush (stdout);
    usleep (sleep_time.tv_sec * 1000000 + sleep_time.tv_usec);
  }

  sigprocmask (SIG_SETMASK, &old_set, NULL);
}

/* Check sleep thread.
 * If it finds an sleep thread, call x_thread_sleep_now (), and move it from SLEEPING to READY
 * self is moved to after_state and swap.
 */
void
x_thread_check_sleep (enum state after_state)
{
  if (schedule [READY] != NULL)
    return;
  if (schedule [SLEEPING] == NULL)
    return;

  struct queue *self, *sleeping;
  struct Thread *sleeping_th;
  struct timeval *awake_time;

  self = x_thread_find (x_thread_self (), RUNNING);
  sleeping = x_thread_extract_shortest_time_sleeper ();

  sleeping_th = sleeping->content;
  awake_time = sleeping_th->awake_time;
  x_thread_sleep_now (awake_time);
  sleeping_th->awake_time = NULL;

  x_thread_move (sleeping, SLEEPING, READY);
  x_thread_swap (self, RUNNING, after_state);
}

/* Called when running-thread will sleep and there aren't ready-threads.
 * If running-thread is the shortest-time sleeper, call x_thread_sleep_now () and resume.
 * If one of sleeping-threads is the shortest-time sleeper, call x_thread_check_sleep ().
 */
void
x_thread_self_sleep (void)
{
  struct queue *self, *sleeping;
  struct Thread *self_th, *sleeping_th;
  struct timeval *awake_time, now;

  self    = x_thread_find (x_thread_self (), RUNNING);
  self_th = self->content;
  if (self_th->awake_time == NULL)
    return;
  sleeping = x_thread_find_shortest_time_sleeper ();
  if (sleeping == NULL || timercmp (self_th->awake_time, ((struct Thread *)sleeping->content)->awake_time, <)) {
    /* self is shortest-time sleeper */
    awake_time = self_th->awake_time;
    x_thread_sleep_now (awake_time);
    self_th->awake_time = NULL;
  } else {
    x_thread_check_sleep (SLEEPING);
  }
}


/* finalize x_thread.
 * If the thread is not last one, push it to the schedule [ZOMBIE]
 *  and run a thread from READY.
 * If the thread is last one, free all threads and call exit ().
 */
void
x_thread_exit (int status)
{
  struct queue *self = x_thread_find (x_thread_self (), RUNNING);
  struct Thread *self_th = self->content;
  if (self_th->join_tid != -1) {
    struct queue *join_queue = x_thread_find (self_th->join_tid, BLOCKED);
    if (join_queue != NULL) {
      x_thread_move (join_queue, BLOCKED, READY);
    }
  }

  if (schedule [READY] == NULL) {
    /* Final Thread */
    x_thread_check_sleep (ZOMBIE);
    queue_free (&schedule [RUNNING]);
    queue_free (&schedule [ZOMBIE]);
    exit (status);
  }

  x_thread_swap (self, RUNNING, ZOMBIE);
}


/* Move running thread from before_state to after_state.
 * Set new running thread from READY.
 * Swapcontext from old to new. 
 */
void
x_thread_swap (struct queue *running, enum state before_state, enum state after_state)
{
  if (schedule [READY] == NULL)
    if (after_state == READY)
      return;
    else if (after_state == SLEEPING) {
      x_thread_self_sleep ();
      return;
    } else if (schedule [SLEEPING] != NULL) {
      x_thread_check_sleep (after_state);
      return;
    } else
      x_thread_exit (0);

  struct queue *new;
  struct Thread *old_th, *new_th;

  queue_extract (&schedule [before_state], running);
  queue_push (&schedule [after_state], running);
  new = queue_pop (&schedule [READY]);
  queue_push (&schedule [RUNNING], new);
  old_th = running->content;
  new_th = new->content;
  swapcontext (&old_th->uc, &new_th->uc);
}

/* move queue from before_state to after_state */
void
x_thread_move (struct queue *moved, enum state before_state, enum state after_state)
{
  queue_extract (&schedule [before_state], moved);
  queue_push (&schedule [after_state], moved);
}

/* switch the running thread to one of ready threads. */
void
x_thread_yield (void)
{
  if (schedule [READY] == NULL)
    return;
  if (schedule [RUNNING] == NULL)
    return;

  x_thread_swap (schedule [RUNNING], RUNNING, READY);
}

/* return the running thread's tid */
int
x_thread_self (void)
{
  struct Thread *running = schedule [RUNNING]->content;
  return running->tid;
}

struct queue *
x_thread_find (int tid, enum state state)
{
  return x_thread_find_from_head (tid, schedule [state]);
}

struct queue *
x_thread_find_from_head (int tid, struct queue *head)
{
  if (head == NULL)
    return NULL;

  struct queue  *scanner;
  struct Thread *scanner_th;
  for (scanner = head; scanner != NULL; scanner = scanner->next) {
    scanner_th = scanner->content;
    if (scanner_th->tid == tid)
      return scanner;
  }

  return NULL;
}

struct queue *
x_thread_find_from_all_state (int tid)
{
  for (int state = 0; state < STATE_NUM; state++) {
    struct queue *tid_queue = x_thread_find (tid, (enum state)state);
    if (tid_queue != NULL)
      return tid_queue;
  }

  return NULL;
}

/* return exit_status */
int
x_thread_free (struct queue *freed, enum state state)
{
  queue_extract (&schedule [state], freed);
  struct Thread *freed_th = freed->content;
  int exit_status = freed_th->exit_status;
  free (freed_th);
  free (freed);
  return exit_status;
}

/* suspend tid_thread until it's resumed */
void
x_thread_suspend (int tid)
{
  struct queue *tid_queue;
  if ((tid_queue = x_thread_find (tid, RUNNING)) != NULL) {
    /* tid_thread is running */
    x_thread_swap (tid_queue, RUNNING, BLOCKED);
    return;
  }

  if ((tid_queue = x_thread_find (tid, READY)) != NULL) {
    /* tid_thread is ready */
    x_thread_move (tid_queue, READY, BLOCKED);
    return;
  }
}

/* resume tid-thread in BLOCKED state */
void
x_thread_resume (int tid)
{
  struct queue *tid_queue = x_thread_find (tid, BLOCKED);
  if (tid_queue == NULL)
    return;

  x_thread_move (tid_queue, BLOCKED, READY);
}

/* join the tid-thread.
 * If the tid-thread is ZOMBIE, free it.
 * If the tid-thread isn't ZOMBIE, running-thread is moved to BLOCKE state
 *   until the tid-thread exits.
 */
int
x_thread_join (int tid)
{
  struct queue *tid_queue = x_thread_find (tid, ZOMBIE);
  if (tid_queue != NULL) {
    /* tid_thread is zombie */
    return x_thread_free (tid_queue, ZOMBIE);
  } else {
    /* tid_thread is not zombie */
    tid_queue = x_thread_find_from_all_state (tid);
    if (tid_queue == NULL)
      return -1;

    struct Thread *tid_thread = tid_queue->content;
    tid_thread->join_tid = x_thread_self ();
    struct queue *self = x_thread_find (x_thread_self (), RUNNING);
    x_thread_swap (self, RUNNING, BLOCKED);

    /* Awaken */
    return x_thread_free (tid_queue, ZOMBIE);
  }
}

/* check whether sleeping-threads should be awaken. 
 * If so, move the threads to READY state.
 */
void
wakeup (void)
{
  if (schedule [SLEEPING] == NULL)
    return;

  struct timeval now;
  gettimeofday (&now, NULL);
  struct queue *scanner;
  struct Thread *scanner_th;
  for (scanner = schedule [SLEEPING]; scanner != NULL; scanner = scanner->next) {
    scanner_th = scanner->content;
    if (timercmp (&now, scanner_th->awake_time, >)) {
      x_thread_move (scanner, SLEEPING, READY);
      // printf ("#%d is wakeup!\n", scanner_th->tid);
      // fflush (stdout);
    }
  }
}

/* calculate awaketime */
struct timeval *
x_thread_awaketime (int sec, int msec)
{
  struct timeval *awake_time, sleep_time, now;
  awake_time = (struct timeval *) malloc (sizeof (struct timeval));
  sleep_time.tv_sec = sec;
  sleep_time.tv_usec = msec;
  gettimeofday (&now, NULL);
  timeradd (&sleep_time, &now, awake_time);
  return awake_time;
}

/* sleep for sec */
void
x_thread_sleep (int sec)
{
  struct queue *self = x_thread_find (x_thread_self (), RUNNING);
  if (self == NULL)
    return;
  struct Thread *self_th = self->content;
  self_th->awake_time = x_thread_awaketime (sec, 0);

  x_thread_swap (self, RUNNING, SLEEPING);
}

/* sleep for msec */
void
x_thread_msleep (int msec)
{
  struct queue *self = x_thread_find (x_thread_self (), RUNNING);
  if (self == NULL)
    return;
  struct Thread *self_th = self->content;
  self_th->awake_time = x_thread_awaketime (0, msec);

  x_thread_swap (self, RUNNING, SLEEPING);
}

ssize_t
x_read (int fd, void *buf, size_t nbytes)
{
  int n;
  while (1) {
    n = read (fd, buf, nbytes);
    if ((n == -1) && (errno == EAGAIN)) {
      /* register fd to TCB */
      struct queue *self = x_thread_find (x_thread_self (), RUNNING);
      struct Thread *self_th = self->content;
      self_th->wait_rfd = fd;
      x_thread_swap (self, RUNNING, BLOCKED);
    } else
      break;
  }
}

ssize_t
x_write (int fd, void *buf, size_t nbytes)
{
  int n;
  while (1) {
    n = write (fd, buf, nbytes);
    if ((n == -1) && (errno == EAGAIN)) {
      /* register fd to TCB */
      struct queue *self = x_thread_find (x_thread_self (), RUNNING);
      struct Thread *self_th = self->content;
      self_th->wait_wfd = fd;
      x_thread_swap (self, RUNNING, BLOCKED);
    } else
      break;
  }
}

/* set max_fd, rfd_set and wfd_set */
void
x_thread_fd_set (int *max_fd, fd_set *rfd_set, fd_set *wfd_set)
{
  FD_ZERO (rfd_set);
  FD_ZERO (wfd_set);
  *max_fd = -1;
  struct queue  *scanner;
  struct Thread *scanner_th;
  int scanner_rfd, scanner_wfd;
  for (scanner = schedule [BLOCKED]; scanner != NULL; scanner = scanner->next) {
    scanner_th = scanner->content;
    scanner_rfd = scanner_th->wait_rfd;
    scanner_wfd = scanner_th->wait_wfd;
    if (scanner_rfd >= 0)
      FD_SET (scanner_rfd, rfd_set);
    if (scanner_wfd >= 0)
      FD_SET (scanner_wfd, wfd_set);

    if (scanner_rfd > *max_fd)
      *max_fd = scanner_rfd;
    if (scanner_wfd > *max_fd)
      *max_fd = scanner_wfd;
  }
}

/* move io-ready-threads from BLOCKED state to READY state */
void
x_thread_io_ready (fd_set *rfd_set, fd_set *wfd_set)
{
  struct queue *scanner;
  struct Thread *scanner_th;
  int scanner_rfd, scanner_wfd;
  for (scanner = schedule [BLOCKED]; scanner != NULL; scanner = scanner->next) {
    scanner_th = scanner->content;
    scanner_rfd = scanner_th->wait_rfd;
    scanner_wfd = scanner_th->wait_wfd;
    if (scanner_rfd != -1 && FD_ISSET (scanner_rfd, rfd_set)) {
      scanner_th->wait_rfd = -1;
      x_thread_move (scanner, BLOCKED, READY);
      // printf ("#%d: BLOCKED -> READY in rfd\n", x_thread_self ());
      // fflush (stdout);
    } else if (scanner_wfd != -1 && FD_ISSET (scanner_wfd, wfd_set)) {
      scanner_th->wait_wfd = -1;
      x_thread_move (scanner, BLOCKED, READY);
    }
  }
}

/* print All threads */
void
x_thread_print_Allthread (void)
{
  sigset_t old_set, new_set;
  sigemptyset (&new_set);
  sigaddset (&new_set, SIGALRM);
  sigprocmask (SIG_SETMASK, &new_set, &old_set);

  printf ("RUNNING : ");
  queue_print (schedule [RUNNING]);
  printf ("READY   : ");
  queue_print (schedule [READY]);
  printf ("ZOMBIE  : ");
  queue_print (schedule [ZOMBIE]);
  printf ("BLOCKED : ");
  queue_print (schedule [BLOCKED]);
  printf ("SLEEPING: ");
  queue_print (schedule [SLEEPING]);
  fflush (stdout);

  sigprocmask (SIG_SETMASK, &old_set, NULL);
}

void
x_signalmask_set (sigset_t *old_ptr, sigset_t *new_ptr)
{
  sigfillset (new_ptr);
  sigdelset (new_ptr, SIGINT);
  sigprocmask (SIG_SETMASK, new_ptr, old_ptr);
}

void
x_signalmask_remove (sigset_t *old_ptr)
{
  sigprocmask (SIG_SETMASK, old_ptr, NULL);
}

/* initialize mutex */
void
x_mutex_init (struct x_mutex *mutex)
{
  mutex->holder = NULL;
  mutex->waiting = NULL;
}

void
x_mutex_lock (struct x_mutex *mutex)
{
  sigset_t old_set, new_set;
  x_signalmask_set (&old_set, &new_set);

  /* self in RUNNING */
  struct queue  *self = x_thread_find (x_thread_self (), RUNNING);
  struct Thread *self_th = self->content;
  while (mutex->holder != NULL) {
    struct queue *wait_self = x_thread_find_from_head (self_th->tid, mutex->waiting);
    if (wait_self == NULL) {
      /* add self to mutex->waiting */
      char name [10];
      snprintf (name, 10, "tid%d", self_th->tid);
      queue_push_new (&mutex->waiting, name, sizeof (name), self_th);
    }
    x_thread_swap (self, RUNNING, BLOCKED);
    x_signalmask_remove (&old_set);

    /* return */
    x_signalmask_set (&old_set, &new_set);
  }
  mutex->holder = self;

  x_signalmask_remove (&old_set);
}

void
x_mutex_unlock (struct x_mutex *mutex)
{
  sigset_t old_set, new_set;
  x_signalmask_set (&old_set, &new_set);

  struct queue  *self = x_thread_find (x_thread_self (), RUNNING);
  struct Thread *self_th = self->content;
  if (self != mutex->holder)
    exit (1);

  mutex->holder = NULL;

  if (mutex->waiting == NULL) {
    x_signalmask_remove (&old_set);
    return;
  }

  struct queue  *scanner, *blocked;
  struct Thread *scanner_th;
  int scanner_tid;
  for (scanner = queue_pop (&mutex->waiting); scanner != NULL; scanner = queue_pop (&mutex->waiting)) {
    scanner_th  = scanner->content;
    scanner_tid = scanner_th->tid;
    blocked = x_thread_find (scanner_tid, BLOCKED);
    if (blocked == NULL)
      continue;
    x_thread_move (blocked, BLOCKED, READY);
    free (scanner);
  }
  mutex->waiting = NULL;

  x_signalmask_remove (&old_set);
}

/* initialize cond */
void
x_cond_init (struct x_cond *cond)
{
  cond->waiting = NULL;
}

void
x_cond_wait (struct x_cond *cond, struct x_mutex *mutex)
{
  struct queue  *self = x_thread_find (x_thread_self (), RUNNING);
  struct Thread *self_th = self->content;
  if (self != mutex->holder)
    exit (1);

  sigset_t old_set, new_set;
  x_signalmask_set (&old_set, &new_set);

  char name [10];
  snprintf (name, 10, "tid%d", self_th->tid);
  queue_push_new (&cond->waiting, name, sizeof (name), self_th);
  x_mutex_unlock (mutex);
  x_thread_swap (self, RUNNING, BLOCKED);
  x_signalmask_remove (&old_set);

  /* RUNNING */
  x_signalmask_set (&old_set, &new_set);
  struct queue *wait_self = x_thread_find_from_head (x_thread_self (), cond->waiting);
  queue_extract (&cond->waiting, wait_self);
  free (wait_self);
  x_signalmask_remove (&old_set);
  x_mutex_lock (mutex);
}

void
x_cond_signal (struct x_cond *cond, struct x_mutex *mutex)
{
  if (cond->waiting == NULL)
    return;

  sigset_t old_set, new_set;
  x_signalmask_set (&old_set, &new_set);

  struct queue *moved, *blocked;
  struct Thread *moved_th;
  int moved_tid;
  moved = queue_pop (&cond->waiting);
  moved_th = moved->content;
  moved_tid = moved_th->tid;
  blocked = x_thread_find (moved_tid, BLOCKED);
  x_thread_move (blocked, BLOCKED, READY);
  free (moved);

  x_signalmask_remove (&old_set);
}

void
x_cond_broadcast (struct x_cond *cond, struct x_mutex *mutex)
{
  if (cond->waiting == NULL)
    return;

  sigset_t old_set, new_set;
  x_signalmask_set (&old_set, &new_set);

  struct queue *scanner, *blocked;
  struct Thread *scanner_th;
  int scanner_tid;
  for (scanner = queue_pop (&cond->waiting); scanner != NULL; scanner = queue_pop (&cond->waiting)) {
    scanner_th  = scanner->content;
    scanner_tid = scanner_th->tid;
    blocked = x_thread_find (scanner_tid, BLOCKED);
    x_thread_move (blocked, BLOCKED, READY);
    free (scanner);
  }

  x_signalmask_remove (&old_set);
}

