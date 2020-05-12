
x_thread : User-thread Library<br>
queue    : queue Library<br>

How to compile<br>
Library (x_thread.c, queue.c) :<br>
% gcc -g -c x_thread.c<br>
% ar -r libx_thread.a x_thread.o<br>

Test :<br>
% gcc -c x_thread_level*.c<br>
% gcc -g -o x_thread_level* x_thread_level*.o libx_thread.a libqueue.a<br>
% ./x_thread_level*<br>
