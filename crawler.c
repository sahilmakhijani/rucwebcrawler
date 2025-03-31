#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Config (Global Variables)
// 1. input_file = urls.txt
// 2. output_directory = current_directory
// 3. important_words = important_words.txt (e.g., data, science, algorithm)
// 4. no_of_threads = 10

// Structs
typedef char* url;
typedef struct url_queue_node_struct {
  url url;
  url_queue_node* next;
} url_queue_node;
typedef struct url_queue_struct {
  url_queue_node* head;
  url_queue_node* tail;
  pthread_mutex_t lock;
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
int enqueue(url_queue* queue, url url);
url dequeue(url_queue* queue);

occurrence_report init_occurrence_report(word* word);
int update_occurrence_report(occurrence_report globalor,
                             occurrence_report localor);

typedef char* content;
content read_file(char* filename);
content fetch(url url);
occurrence_report count_occurrences(content html);
int write_file(char* filename, content);

void* thread_worker(void* arg);

// Implementation
// TODO

int main(int argc, char* argv[]) {
  printf("=== RUC Web Crawler ===\n");

  // Initialize queue
  // Read input file and keep populating the queues
  // Make pthreads, start them and join to wait
  // Free EVERYTHING (make sure no memory leaks!)

  return EXIT_SUCCESS;
}
