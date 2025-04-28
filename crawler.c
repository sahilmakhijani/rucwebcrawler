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
  int failure = -1; //track presence of failure and where
  for (int i = 0; i < count; i++){
    report.word_counts[i].word = strdup(word[i]);
    if(report.word_counts[i].word == NULL){
      // strdup failed
      failure = i;
      break;
    }
    report.word_counts[i].count = 0;
  }

  // If strdup failed, free successful allocations up to fail
  // and return empty report.
  if (failure != -1){
    for (int k = 0; k < failure; k++){
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
  
  // Lock global report, check for errors
  if (pthread_mutex_lock(&globalor.lock) != 0) {
    // failed to lock
    return -1;
  }

  // Check if any reports returned NULL
  if (globalor.word_counts == NULL || localor.word_counts == NULL) {
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

/** HELPER FUNCTION
 * to_lowercase - lowers each character in a character array
 * @str: the array of characters
 */
void to_lowercase(char *str){
  // Because of how ASCII works, we can add 25 to lower any uppercase char
  for(int i = 0; str[i] != '\0'; i++){
    if(str[i] >= 'A' && str[i] <= 'Z')
      str[i] += 32;
  }
}


/**
 * count_occurrences - processes html, counts words
 * @html: the stuff to process
 * @return: the occurrence report with updated counts
 */
occurrence_report count_occurrences(content html){
  
  // Initialize an occurrence report with the important words
  occurrence_report report = init_occurrence_report(important_words);

  if (report.word_counts == NULL || html == NULL){
    printf("Error: bad report or html");
    //TODO: FURTHER ERROR HANDLING
    fprintf(stderr, "Error: report or html is null");
    exit(EXIT_FAILURE);
  }

  // strtok_r for thread safe string tokenizer
  char* current; // current position in the html

  // some delimiters but it's ok not to be super exact
  char* delimiters = ".,-\"';:?!@#/&*()[]{}/\\_~+= \t\r\n";

  // first token
  char* token = strtok_r(html, delimiters, &current);
  

  while(token != NULL){
    // lowercase the word
    to_lowercase(token);

    // iterate through important words
    for(int i = 0; report.word_counts[i].word != NULL; i++){
      // if we get a hit
      if(strcmp(token, report.word_counts[i].word) == 0){
        report.word_counts[i].count++;
        break;
      }
    }

    // next token
    token = strtok_r(current, delimiters, &current);

  }

  // free some local pointers
  free(token);
  free(delimiters);
  free(current);
  
  return report;

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