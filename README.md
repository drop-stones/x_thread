
x_thread : User-thread Library
queue    : queue Library

How to compile
Library (x_thread.c, queue.c) :
% gcc -g -c x_thread.c
% ar -r libx_thread.a x_thread.o

Test :
% gcc -c x_thread_level*.c
% gcc -g -o x_thread_level* x_thread_level*.o libx_thread.a libqueue.a
% ./x_thread_level*
