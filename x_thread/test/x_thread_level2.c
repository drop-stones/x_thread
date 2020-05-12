#include <stdio.h> 	/* fprintf */
#include <unistd.h>     /* sleep */
#include "../x_thread.h"

int
foo (void *arg)
{
  int i = 5;
  while (i--) {
    // fprintf (stderr, "#%d ", x_thread_self ());
    printf ("#%d ", x_thread_self ());
    fflush (stdout);
    for (int j = 0; j < 100000000; j++)
      ;
  }
  return 0;
}

int
main (void)
{
  int i = 5;
    
  x_thread_init ();
  x_thread_create (foo, NULL);
  x_thread_create (foo, NULL);
  while (i--) {
    // fprintf (stderr, "#%d ", x_thread_self ());
    printf ("#%d ", x_thread_self ());
    fflush (stdout);
    for (int j = 0; j < 100000000; j++)
      ;
  }
  x_thread_exit (0);
}
