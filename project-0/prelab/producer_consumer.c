#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Queue Structures */
typedef struct queue_node_s {
  struct queue_node_s *next;
  struct queue_node_s *prev;
  char c;
} queue_node_t;

typedef struct {
  struct queue_node_s *front;
  struct queue_node_s *back;
  pthread_mutex_t lock;
} queue_t;


/* Thread Function Prototypes */
void *producer_routine(void *arg);
void *consumer_routine(void *arg);


/* Global Data */
long g_num_prod; /* number of producer threads */
pthread_mutex_t g_num_prod_lock;


/* Main - entry point */
int main(int argc, char **argv) {
  queue_t queue;
  pthread_t producer_thread, consumer_thread;
  void *thread_return = NULL;
  int result = 0;

  /* Initialization */
  printf("Main thread started with thread id %lu\n", pthread_self());

  memset(&queue, 0, sizeof(queue));
  pthread_mutex_init(&queue.lock, NULL);

  g_num_prod = 1; /* there will be 1 producer thread */

  /* Create producer and consumer threads */
  result = pthread_create(&producer_thread, NULL, producer_routine, &queue);
  if (0 != result) {
    fprintf(stderr, "Failed to create producer thread: %s\n", strerror(result));
    exit(1);
  }

  printf("Producer thread started with thread id %lu\n", producer_thread);

  /*
  BUG FIX 01: do not use pthread_detach() for producer. 
    Each thread must be managed using either pthread_detach() or pthread_join().
    By using pthread_join(), the program can ensure that the producer completes adding all alphabets to the queue before proceeding. 
    If pthread_detach() is used and the producer fails unexpectedly, the program may hang indefinitely as consumers rely on g_num_prod > 0 to terminate.
   */
  // result = pthread_detach(producer_thread);
  // if (0 != result)
  //   fprintf(stderr, "Failed to detach producer thread: %s\n", strerror(result));

  result = pthread_create(&consumer_thread, NULL, consumer_routine, &queue);
  if (0 != result) {
    fprintf(stderr, "Failed to create consumer thread: %s\n", strerror(result));
    exit(1);
  }

  /* Join threads, handle return values where appropriate */
  result = pthread_join(producer_thread, NULL);
  if (0 != result) {
    fprintf(stderr, "Failed to join producer thread: %s\n", strerror(result));
    pthread_exit(NULL);
  }

  result = pthread_join(consumer_thread, &thread_return);
  if (0 != result) {
    fprintf(stderr, "Failed to join consumer thread: %s\n", strerror(result));
    pthread_exit(NULL);
  }
  printf("outer-consumer printed %lu characters.\n", *(long*)thread_return);
  free(thread_return);

  pthread_mutex_destroy(&queue.lock);
  pthread_mutex_destroy(&g_num_prod_lock);
  return 0;
}


/* Function Definitions */

/* producer_routine - thread that adds the letters 'a'-'z' to the queue */
void *producer_routine(void *arg) {
  queue_t *queue_p = arg;
  queue_node_t *new_node_p = NULL;
  pthread_t consumer_thread;
  void *consumer_return = NULL;
  int result = 0;
  char c;

  result = pthread_create(&consumer_thread, NULL, consumer_routine, queue_p);
  if (0 != result) {
    fprintf(stderr, "Failed to create consumer thread: %s\n", strerror(result));
    exit(1);
  }

  /*
  BUG FIX 02: do not use pthread_detach() for consumer. 
    Each thread must be managed using either pthread_detach() or pthread_join(). 
    Instead, we join the producer_thread later. This allow us to retrieve the count of alphabets consumed by the consumer thread. 
    Using pthread_detach() would prevent us from accessing this variable, as there would be no way to join. 
   */
  // result = pthread_detach(consumer_thread);
  // if (0 != result)
  //   fprintf(stderr, "Failed to detach consumer thread: %s\n", strerror(result));

  for (c = 'a'; c <= 'z'; ++c) {

    /* Create a new node with the prev letter */
    new_node_p = malloc(sizeof(queue_node_t));
    new_node_p->c = c;
    new_node_p->next = NULL;

    /* Add the node to the queue */
    pthread_mutex_lock(&queue_p->lock);
    if (queue_p->back == NULL) {
      assert(queue_p->front == NULL);
      new_node_p->prev = NULL;

      queue_p->front = new_node_p;
      queue_p->back = new_node_p;
    }
    else {
      assert(queue_p->front != NULL);
      new_node_p->prev = queue_p->back;
      queue_p->back->next = new_node_p;
      queue_p->back = new_node_p;
    }
    pthread_mutex_unlock(&queue_p->lock);

    sched_yield();
  }

  /* Decrement the number of producer threads running, then return */
  /*
  BUG FIX 03: decrement g_num_prod atomically.
    In programs multiple producer threads, a race condition can occur during the decrement of g_num_prod. 
    This may result in an incorrect value, potentially leaving g_num_prod greater than zero, causing consumers to run indefinitely (g_num_prod > 0). 
    Using proper synchronization prevents this issue.
  */
  pthread_mutex_lock(&g_num_prod_lock);
  --g_num_prod;
  pthread_mutex_unlock(&g_num_prod_lock);

  result = pthread_join(consumer_thread, &consumer_return);
  if (0 != result) {
    fprintf(stderr, "Failed to join consumer thread: %s\n", strerror(result));
    exit(1);
  }
  printf("\ninner-consumer printed %ld characters.\n", *(long *)consumer_return);
  free(consumer_return);

  return (void*) 0;
}


/* consumer_routine - thread that prints characters off the queue */
void *consumer_routine(void *arg) {
  queue_t *queue_p = arg;
  queue_node_t *prev_node_p = NULL;
  
  /*
  BUG FIX 04: allocate 'count' in heap memory instead of stack memory. 
    In C, variables declared on the stack are automatically deallocated when the function returns. 
    To ensure that 'count' remains valid and accessible by the caller (e.g., via pthread_join), it must be allocated in heap memory using malloc. 
    The caller is responsible for freeing the allocated memory to avoid memory leaks.
  */ 
  // long count = 0; /* number of nodes this thread printed */
  long *count = malloc(sizeof(long)); 
  *count = 0;

  printf("Consumer thread started with thread id %lu\n", pthread_self());

  /* terminate the loop only when there are no more items in the queue
   * AND the producer threads are all done */


  /*
  BUG FIX 05: use queue_p->lock to ensure queue operations are handled by only one consumer at a time.
    The previous implementation is vulnerable to race conditions, where multiple consumer threads 
    could iterate and perform queue operations concurrently.
    By using pthread_mutex_lock() and pthread_mutex_unlock() for each iteration, we ensure that only one thread
    can access and modify the queue at a time. This atomic control prevents concurrent access issues and 
    maintains the integrity of the queue during operations.
   */
  while(queue_p->front != NULL || g_num_prod > 0) {
    // pthread_mutex_unlock(&g_num_prod_lock);
    pthread_mutex_lock(&queue_p->lock);

    if (queue_p->front != NULL) {

      /* Remove the prev item from the queue */
      prev_node_p = queue_p->front;

      if (queue_p->front->next == NULL)
        queue_p->back = NULL;
      else
        queue_p->front->next->prev = NULL;

      queue_p->front = queue_p->front->next;
      
      /* Print the character, and increment the character count */
      printf("%c", prev_node_p->c);
      free(prev_node_p);
      ++(*count);
    }
    else { /* Queue is empty, so let some other thread run */
      sched_yield();
    }
    pthread_mutex_unlock(&queue_p->lock);
  }

  return count;
}
