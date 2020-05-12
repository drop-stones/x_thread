#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

void
queue_print (struct queue *head)
{
  if (head == NULL) {
    printf ("\n");
    return;
  }

  struct queue *scanner = head;
  printf ("%s", scanner->name);
  while (scanner->next != NULL) {
    scanner = scanner->next;
    printf (" -> %s", scanner->name);
  }
  printf ("\n");
}

/* initialize head node */
void
queue_init (struct queue **head, char *name, size_t name_len, void *content)
{
  *head = (struct queue *) malloc (sizeof (struct queue));
  strncpy ((*head)->name, name, name_len);
  (*head)->content = content;
  (*head)->prev = NULL;
  (*head)->next = NULL;
}

/* create new node and push it */
void
queue_push_new (struct queue **head, char *name, size_t name_len, void *content)
{
  if (*head == NULL) {
     queue_init (head, name, name_len, content);
     return;
   }

  struct queue *new  = (struct queue *) malloc (sizeof (struct queue));
  strncpy (new->name, name, name_len);
  new->content = content;

  queue_push (head, new);
}

/* push the queue passed to this function */
void
queue_push (struct queue **head, struct queue *pushed_queue)
{
  if (*head == NULL) {
    *head = pushed_queue;
    return;
  }

  struct queue *back = queue_back (*head);
  back->next = pushed_queue;
  pushed_queue->prev = back;
  pushed_queue->next = NULL;
}

/* pop head node */
struct queue *
queue_pop (struct queue **head)
{
  if (*head == NULL)
    return NULL;

  if ((*head)->next == NULL) {
    struct queue *popped_node = *head;
    *head = NULL;
    return popped_node;
  }

  struct queue *popped_node = *head;
  *head = (*head)->next;
  popped_node->prev = NULL;
  popped_node->next = NULL;
  return popped_node;
}

/* return pointer to back_node */
struct queue *
queue_back (struct queue *head)
{
  if (head == NULL)
    return NULL;

  struct queue *scanner = head;
  while (scanner->next != NULL)
    scanner = scanner->next;

  return scanner;
}

/* free all node */
void
queue_free (struct queue **head)
{
  if (*head == NULL)
    return;

  struct queue *scanner = *head;
  while (scanner->next != NULL) {
    scanner = scanner->next;
    if (scanner->prev->content != NULL)
      free (scanner->prev->content);
    free (scanner->prev);
  }
  if (scanner->content != NULL)
    free (scanner->content);
  free (scanner);
}

/* cut links around extracted_node, and reconnect prev <-> next link */
void
queue_extract (struct queue **head, struct queue *extracted)
{
  if (*head == NULL || extracted == NULL)
    return;

  if (*head == extracted) {
    if (extracted->next == NULL) {
      *head = NULL;
      return;
    }
    struct queue *next = extracted->next;
    *head = next;
    extracted->prev = NULL;
    extracted->next = NULL;
    next->prev = NULL;
    return;
  }

  struct queue *prev = extracted->prev;
  struct queue *next = extracted->next;
  if (prev != NULL)
    prev->next = next;
  if (next != NULL)
    next->prev = prev;
  extracted->prev = NULL;
  extracted->next = NULL;
}
