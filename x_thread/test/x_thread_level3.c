#include <stdio.h>
#include "../x_thread.h"

int
func_suspend (void *arg)
{
  printf ("#%d suspend #3\n", x_thread_self ());
  fflush (stdout);
  x_thread_suspend (3);
  x_thread_sleep (2);
  printf ("#%d resume #3\n", x_thread_self ());
  fflush (stdout);
  x_thread_resume (3);
  return 0;
}

int
func_sleep (void *arg)
{
  printf ("#%d is sleep for 5 sec .zZ\n", x_thread_self ());
  fflush (stdout);
  x_thread_sleep (5);
  printf ("#%d is wakeup!\n", x_thread_self ());
  fflush (stdout);
  return 0;
}

int
func_waited (void *arg)
{
  printf ("#%d is finish!\n", x_thread_self ());
  fflush (stdout);
  return 0;
}

int
main (int argc, char *argv [])
{
  x_thread_init ();
  x_thread_create (func_suspend, NULL);
  x_thread_create (func_sleep, NULL);
  x_thread_create (func_waited, NULL);

  printf ("#%d will join to #3\n", x_thread_self ());
  fflush (stdout);
  x_thread_join (3);
  printf ("#%d have joined to #3!\n", x_thread_self ());
  fflush (stdout);

  x_thread_exit (0);
}
