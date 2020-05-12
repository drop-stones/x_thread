#include <stdio.h>
#include "../x_thread.h"

static int condition = 1;

int
func (void *arg)
{
  char buf [10];
  printf ("\n#%d: wait for read.\n", x_thread_self ());
  fflush (stdout);
  x_read (STDIN_FILENO, buf, 9);
  printf ("\n#%d: wait for write.\n", x_thread_self ());
  fflush (stdout);
  x_write (STDOUT_FILENO, buf, 9);
  condition = 0;
  return 0;
}

int
main (void)
{
  x_thread_init ();
  x_thread_create (func, NULL);
  while (condition) {
    printf ("#%d ", x_thread_self ());
    fflush (stdout);
    x_thread_sleep (1);
  }

  x_thread_exit (0);
}
