#include <sys/types.h>
#include <unistd.h>	/* size_t */
#define NAME_LEN 256

struct queue {
  char name[NAME_LEN];
  void *content;
  struct queue *prev;
  struct queue *next;
};

void          queue_print	(struct queue *head);		/* print All name to stdout */
void 	      queue_init	(struct queue **head, char *name, size_t name_len, void *content);
/* create new queue and push it */
void	      queue_push_new 	(struct queue **head, char *name, size_t name_len, void *content);
/* push the queue */
void	      queue_push	(struct queue **head, struct queue *pushed_queue);
struct queue *queue_pop 	(struct queue **head);		/* delete head and return it */
struct queue *queue_back	(struct queue *head);		/* return tail queue */
void	      queue_free   	(struct queue **head);		/* free all node */
/* extract the node from linked list */
void	      queue_extract	(struct queue **head, struct queue *extracted);
