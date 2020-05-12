#include <stdio.h>
#include "../x_thread.h"

static struct x_mutex mutex;
static struct x_cond  empty, full;

static int N = 3, counter = 0;

int
producer (void *arg)
{
  while (1) {
    x_mutex_lock (&mutex);
    while (counter == N)
      x_cond_wait (&full, &mutex);
    counter++;
    printf ("producer: %d\n", counter);
    fflush (stdout);
    x_cond_broadcast (&empty, &mutex);
    x_mutex_unlock (&mutex);
  }
}

int
consumer (void *arg)
{
  while (1) {
    x_mutex_lock (&mutex);
    while (counter == 0)
      x_cond_wait (&empty, &mutex);
    counter--;
    printf ("consumer: %d\n", counter);
    x_cond_broadcast (&full, &mutex);
    x_mutex_unlock (&mutex);
  }
}

int
main (int argc, char *argv [])
{
  x_thread_init ();
  x_mutex_init (&mutex);
  x_cond_init (&empty);
  x_cond_init (&full);
  x_thread_create (producer, NULL);
  x_thread_create (consumer, NULL);
  while (1)
    ;

  x_thread_exit (0);
}

