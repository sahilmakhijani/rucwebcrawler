#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Config (Global Variables)
char* input_file = "urls.txt";
char* output_directory = "./";
char* important_words = "important_words.txt";
int no_of_threads = 10;

// Structs
typedef char* url;
typedef struct url_queue_node_struct {
  url url;
  struct url_queue_node_struct* next;
} url_queue_node;
typedef struct url_queue_struct {
  url_queue_node* head;
  url_queue_node* tail;
  pthread_mutex_t lock;
  int size;
} url_queue;

typedef char* word;
typedef struct word_count_struct {
  word word;
  int count;
} word_count;
typedef struct occurrence_report_struct {
  word_count* word_counts;
  pthread_mutex_t lock;
} occurrence_report;

// Function Declarations
url_queue* init_url_queue();
url_queue_node* init_queue_node(url data);
int enqueue_url(url_queue* queue, url data);
url dequeue_url(url_queue* queue);
void free_url_queue(url_queue* queue);
bool isEmpty(url_queue* queue);
void test_Queue();

occurrence_report init_occurrence_report(word* word);
int update_occurrence_report(occurrence_report globalor,
                             occurrence_report localor);
void free_occurence_report(occurrence_report or);

typedef char* content;
content read_file(char* filename);
content fetch(url url);
occurrence_report count_occurrences(content html);
int write_file(char* filename, content);

void init_impwords_file();  // (e.g., data, science, algorithm)

void* thread_worker(void* args);  // args[0] = queue and args[1] = globalor

// Implementation
// TODO

/*************************************************************
 * QUEUE OPERATIONS
 ************************************************************/
/**
 * init_url_queue - Initializes url_queue Struct
 * @return: Pointer to initialized queue
 */
url_queue* init_url_queue() {
  url_queue* q = (url_queue*)malloc(sizeof(url_queue));
  q->head = NULL;
  q->tail = NULL;
  pthread_mutex_init(&q->lock, NULL);
  q->size = 0;

  return q;
}

/**
 * init_queue_node - Initializes url_queue_node Struct
 * @data: URL String to be stored
 * @return: Pointer to initialized node
 */
url_queue_node* init_queue_node(url data) {
  url_queue_node* node = (url_queue_node*)malloc(sizeof(url_queue_node));
  node->url = data;
  node->next = NULL;

  return node;
}

/**
 * isEmpty - Checks if Queue is empty
 * @queue: Pointer to queue to be checked
 * @return: True if queue is empty; False otherwise
 */
bool isEmpty(url_queue* queue) {
  return (queue->head == NULL);
}

/**
 * enqueue_url - Enqueues node containing URL to queue
 * @queue: Pointer to queue that will be enqueued to
 * @data: URL String to be enqueued
 * @return: 0 if enqueue is successful
 */
int enqueue_url(url_queue* queue, url data) {
  url_queue_node* node = init_queue_node(data);
  pthread_mutex_lock(&queue->lock);
  // Base Case
  if (isEmpty(queue)) {
    queue->head = node;
  }
  // Append new node to queue's tail
  else {
    queue->tail->next = node;
  }

  // Set new node to tail
  queue->tail = node;
  queue->size = queue->size + 1;
  pthread_mutex_unlock(&queue->lock);
  return 0;
}

/**
 * dequeue_url - Dequeues node at the head of Queue
 * @queue: Pointer to queue that will be dequeued to
 * @return: URL String of dequeued node
 */
url dequeue_url(url_queue* queue) {
  // Base case
  if (isEmpty(queue)) {
    return NULL;
  }

  pthread_mutex_lock(&queue->lock);
  url_queue_node* node = queue->head;
  url dequeued_data = node->url;

  // Move head to next node in Queue
  queue->head = queue->head->next;
  queue->size = queue->size - 1;

  // If Queue is now empty
  if (queue->head == NULL) {
    queue->tail = NULL;
  }

  free(node);
  pthread_mutex_unlock(&queue->lock);

  return dequeued_data;
}

/**
 * free_url_queue - Deallocates memory of queue and its nodes
 * @queue: Pointer to queue that will be freed
 */
void free_url_queue(url_queue* queue) {
  // Free each remaining node in queue
  while (!isEmpty(queue)) {
    url dequeued_data = dequeue_url(queue);
    free(dequeued_data);
  }

  free(queue);
  return;
}

/*************************************************************
 * MAIN METHOD
 ************************************************************/
int main(int argc, char* argv[]) {
  printf("=== RUC Web Crawler ===\n");

  // Initialize queue
  // Read input file and keep populating the queues
  // Make pthreads, start them and join to wait
  // Free EVERYTHING (make sure no memory leaks!)

  return EXIT_SUCCESS;
}

/*************************************************************
 * TESTING FUNCTIONS
 ************************************************************/
void test_Queue() {
  int MAX_CHAR_SIZE = 50;
  url_queue* queue = init_url_queue();
  if (isEmpty(queue)) {
    printf("Queue is empty\n");
  }

  url data1 = (url)malloc((MAX_CHAR_SIZE + 1) * sizeof(url));
  printf("Insert a string (MAX %d CHARS): ", MAX_CHAR_SIZE);
  scanf("%s", data1);
  enqueue_url(queue, data1);
  printf("Size is: %d\n", queue->size);

  url data2 = (url)malloc((MAX_CHAR_SIZE + 1) * sizeof(url));
  printf("Insert another string (MAX %d CHARS): ", MAX_CHAR_SIZE);
  scanf("%s", data2);
  enqueue_url(queue, data2);
  printf("Size is: %d\n", queue->size);

  url pop1 = dequeue_url(queue);
  printf("Popped String is: %s\n", pop1);
  free(pop1);
  printf("Size is: %d\n", queue->size);

  if (isEmpty(queue)) {
    printf("Queue is empty\n");
  } else {
    printf("Queue is NOT empty yet\n");
  }

  free_url_queue(queue);
}