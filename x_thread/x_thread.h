#include <sys/types.h>  
#include <unistd.h>     /* ssize_t, size_t */
#include <ucontext.h> 	/* ucontext_t */
#include <pthread.h>    /* pthread */
#include <sys/time.h>	/* timeval */

struct Thread {
  int tid;              	/* thread ID */
  ucontext_t uc;        	/* thread context */
  int exit_status;      	/* exit code */
  int join_tid;			/* thread ID of join */
  struct timeval *awake_time;	/* start time of sleep */
  int wait_rfd;			/* wait read fd */
  int wait_wfd;			/* wait write fd */
  // struct Thread *prev;  	/* for scheduling queue */
  // struct Thread *next;  	/* for scheduling queue */
};

struct x_mutex {
  struct queue *holder;		/* Lock holder */
  struct queue *waiting;	/* Lock waiting Threads list */
};

struct x_cond {
  struct queue *waiting;	/* List of Threads waiting notifications of condition */
};

void x_thread_init    (void);
int  x_thread_create  (int (*func)(void *arg), void *arg);
void x_thread_exit    (int status);
void x_thread_yield   (void);
int  x_thread_self    (void);

void x_thread_suspend (int tid);
void x_thread_resume  (int tid);
int  x_thread_join    (int tid);
void x_thread_sleep   (int sec);
void x_thread_msleep  (int msec);

ssize_t x_read  (int fd, void *buf, size_t nbytes);
ssize_t x_write (int fd, void *buf, size_t nbytes);

void x_mutex_init     (struct x_mutex *mutex);
void x_mutex_lock     (struct x_mutex *mutex);
void x_mutex_unlock   (struct x_mutex *mutex);

void x_cond_init      (struct x_cond *cond);
void x_cond_wait      (struct x_cond *cond, struct x_mutex *mutex);
void x_cond_signal    (struct x_cond *cond, struct x_mutex *mutex);
void x_cond_broadcast (struct x_cond *cond, struct x_mutex *mutex);

void x_thread_print_Allthread (void);
