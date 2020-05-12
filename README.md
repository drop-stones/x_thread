
x_thread : User-thread Library<br>
queue    : queue Library<br>

### How to compile<br>
Library (x_thread.c, queue.c) :<br>
```bash
% gcc -g -c x_thread.c
% ar -r libx_thread.a x_thread.o
```
Test :<br>
```bash
% gcc -c x_thread_level*.c
% gcc -g -o x_thread_level* x_thread_level*.o libx_thread.a libqueue.a
% ./x_thread_level*
```
