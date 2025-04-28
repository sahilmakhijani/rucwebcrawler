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
  url_queue_node* next;
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
int enqueue_url(url_queue* queue, url url);
url dequeue_url(url_queue* queue);
void free_url_queue(url_queue* queue);

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

/************************************ 
 * OCCURRENCE REPORTS
 ************************************/

/**
 * init_occurrence_report - Initializes occurrence_report struct
 * @word : important words array
 * @return: the occurrence report
 */
occurrence_report init_occurrence_report(word* word){

  // Initialize a blank report
  occurrence_report report;
  report.word_counts = NULL;
  // We don't want a null report, so we can check if a report is
  // null and catch an error that way.

  if (pthread_mutex_init(&report.lock, NULL) != 0){
    // Mutex failed to initialize, return it empty
    return report;
  }

  // Count for malloc
  int count = 0;
  while (word[count] != NULL && word[count][0] != '\0') {
    count++;
  }

  // malloc the tracked words by count. note delimiter
  report.word_counts = malloc(sizeof(word_count) * (count + 1));
  if (report.word_counts == NULL ){
    // malloc failed
    return report;
  }

  // Populate malloc with word and 0 count
  int j = -1;
  for (int i = 0; i < count; i++){
    report.word_counts[i].word = strdup(word[i]);
    if(report.word_counts[i].word == NULL){
      // strdup failed
      j = i;
      break;
    }
    report.word_counts[i].count = 0;
  }

  // If strdup failed, free successful allocations up to fail
  // and return empty report.
  if (j != -1){
    for (int k = 0; k < j; k++){
      // freeing the words
      free(report.word_counts[k].word);
    }
    // free array, return empty report
    free(report.word_counts);
    report.word_counts = NULL;
    return report;
  }

  // Delimiter for the word_counts in the report.
  report.word_counts[count].word = NULL;
  report.word_counts[count].count = 0; // Not needed but to avoid garbage


  return report;
}

/**
 * update_occurrence_report - update the global report with input from a local one
 * @globalor: the global occurrence report
 * @localor: the individual occurrence report a thread is currently handling
 * @return: 0 if successful, -1 otherwise
 */
int update_occurrence_report(occurrence_report globalor, occurrence_report localor){
  
  // Lock both locks, check for errors
  if (pthread_mutex_lock(&localor.lock) != 0) {
    pthread_mutex_unlock(&globalor.lock);
    return -1;
  }

  // Check if any reports returned NULL
  if (globalor.word_counts == NULL || localor.word_counts == NULL) {
    pthread_mutex_unlock(&localor.lock);
    pthread_mutex_unlock(&globalor.lock);
    return -1;
  }


  // All reports should track all impwords in the same order.
  int i = 0;
  while(globalor.word_counts[i].word != NULL){
    globalor.word_counts[i].count += localor.word_counts[i].count;
    i++;
  }

  // Release the thread locks.
  pthread_mutex_unlock(&globalor.lock);
  pthread_mutex_unlock(&localor.lock); 
  
  return 0; // success
}

/**
 * free_occurrence_report - frees the occurrence report struct
 * @or: the report to be freed
 */
void free_occurence_report(occurrence_report or){
  // Free each word first
  int i = 0;
  while (or.word_counts[i].word != NULL){
    free(or.word_counts[i].word);
    i++;
  }

  // Free the word counts array
  free(or.word_counts);

  // Destroy the lock
  pthread_mutex_destroy(&or.lock);

}

/***********************************************************/


int main(int argc, char* argv[]) {
  printf("=== RUC Web Crawler ===\n");

  // Initialize queue
  // Read input file and keep populating the queues
  // Make pthreads, start them and join to wait
  // Free EVERYTHING (make sure no memory leaks!)

  return EXIT_SUCCESS;
}