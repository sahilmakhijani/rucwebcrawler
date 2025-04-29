#include <ctype.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Config (Global Variables)
char* urls_file = "urls.txt";
char* html_directory = "./";
char* impwords_file = "impwords.txt";
int threads_count = 10;

// Structs
typedef char* url;
typedef struct url_queue_node_struct {
  url url;
  struct url_queue_node_struct* next;
  int index;
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
url_queue_node* dequeue_url(url_queue* queue);
void free_url_queue(url_queue* queue);
bool isEmpty(url_queue* queue);

occurrence_report* init_occurrence_report();
occurrence_report* count_occurrences(char* html);
int update_occurrence_report(occurrence_report* globalor,
                             occurrence_report* localor);
void free_occurrence_report(occurrence_report* or);
void log_occurrence_report(occurrence_report* or);

char* read_file(char* filename);
char* fetch(url url);
int write_file(char* filename, char* data);

void init_impwords_file();  // (e.g., data, science, algorithm)
word* get_impwords();

void* thread_worker(void* args);

/*************************************************************
 * HELPERS
 ************************************************************/
char* strdup(const char* str) {
  int n = strlen(str) + 1;
  char* dup = malloc(n);
  if (dup) {
    strcpy(dup, str);
  }
  return dup;
}

bool startswith(char* str, char* prefix) {
  for (int i = 0; prefix[i] != '\0'; i++) {
    char char1 = prefix[i];
    char char2 = str[i];
    if (char1 >= 'A' && char1 <= 'Z')
      char1 += 32;
    if (char2 >= 'A' && char2 <= 'Z')
      char2 += 32;
    if (char1 != char2)
      return false;
  }
  return true;
}

/*************************************************************
 * FILE OPS
 ************************************************************/

// Memory struct
typedef struct MemoryStruct {
  char* memory;
  size_t size;
} MemoryStruct;

// Function to read a file and return its contents
char* read_file(char* filename) {
  FILE* file = fopen(filename, "r");
  // error handling for FNF / wrong file name
  if (!file) {
    fprintf(stderr, "Error: Could not open file %s\n", filename);
    return NULL;
  }

  // get the size of the file
  fseek(file, 0, SEEK_END);
  long filesize = ftell(file);
  rewind(file);

  // error handling for empty file
  if (filesize <= 0) {
    fprintf(stderr, "Error: File %s is empty or invalid\n", filename);
    fclose(file);
    return NULL;
  }

  // dynamically allocate memory
  char* buffer = (char*)malloc(filesize + 1);
  // error handling for memory
  if (!buffer) {
    fprintf(stderr, "Error: Memory allocation failed for file %s\n", filename);
    fclose(file);
    return NULL;
  }

  // reads the entire file into buffer
  size_t read_size = fread(buffer, 1, filesize, file);
  // error handling if read fails
  if (read_size != filesize) {
    fprintf(stderr, "Error: Failed to read complete file %s\n", filename);
    free(buffer);
    fclose(file);
    return NULL;
  }

  buffer[filesize] = '\0';

  fclose(file);
  return buffer;
}

// Callback function for libcurl
// Helps curl store downloaded data into memory
static size_t WriteMemoryCallback(void* contents,
                                  size_t size,
                                  size_t nmemb,
                                  void* userp) {
  size_t real_size = size * nmemb;  // to get the # of bytes
  // cast back userp to MemoryStruct*
  struct MemoryStruct* mem = (struct MemoryStruct*)userp;

  // reallocate the buffer to hold new + previous data
  char* ptr = realloc(mem->memory, mem->size + real_size + 1);
  // error handling for memory
  if (!ptr) {
    fprintf(stderr, "Error: Not enough memory (realloc returned NULL)\n");
    return 0;
  }

  // copies the data into right position inside the buffer
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, real_size);
  // update the size of buffer
  mem->size += real_size;
  mem->memory[mem->size] = '\0';
  return real_size;
}

// Function to fetch HTML content from a URL as a char*
char* fetch(url url) {
  CURL* curl_handle;
  CURLcode res;
  // initialize empty chunk to hold new data
  struct MemoryStruct chunk;
  chunk.memory = malloc(1);
  chunk.size = 0;

  // starts a new curl easy session
  curl_handle = curl_easy_init();
  // error handling if curl failed to initialize
  if (!curl_handle) {
    fprintf(stderr, "Error: Failed to initialize curl\n");
    return NULL;
  }
  // sets the URL to fetch
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);
  // this tells curl to use our WriteMemCallback to store new data
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  // gives curl a pointer to chunk to write new data
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);

  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "rucwebcrawler/1.0");

  // HTTP GET request
  res = curl_easy_perform(curl_handle);
  // if something goes wrong, we free the memory and cleanup
  if (res != CURLE_OK) {
    fprintf(stderr, "Error: curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    free(chunk.memory);
    curl_easy_cleanup(curl_handle);
    return NULL;
  }

  curl_easy_cleanup(curl_handle);

  return chunk.memory;
}

// WRITE FUNCTION
// takes in char content in the memory
int write_file(char* filename, char* data) {
  FILE* file = fopen(filename, "w");
  // error handling
  if (!file) {
    fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
    return -1;
  }

  // gets the length of data we want to write
  size_t data_length = strlen(data);
  // size of each item is 1 bytes, how many items to write, and file
  size_t written = fwrite(data, 1, data_length, file);
  if (written != data_length) {
    fprintf(stderr, "Error: Failed to write complete data to %s\n", filename);
    fclose(file);
    return -1;
  }

  fclose(file);
  return 0;  // success
}

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
  node->index = -1;

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
  node->index = queue->size;
  pthread_mutex_unlock(&queue->lock);
  return 0;
}

/**
 * dequeue_url - Dequeues node at the head of Queue
 * @queue: Pointer to queue that will be dequeued to
 * @return: Dequeued node containing url and it's index
 */
url_queue_node* dequeue_url(url_queue* queue) {
  // Base case
  if (isEmpty(queue)) {
    return NULL;
  }

  pthread_mutex_lock(&queue->lock);
  url_queue_node* dequeued_node = queue->head;

  // Move head to next node in Queue
  queue->head = queue->head->next;
  queue->size = queue->size - 1;

  // If Queue is now empty
  if (queue->head == NULL) {
    queue->tail = NULL;
  }

  dequeued_node->next = NULL;
  pthread_mutex_unlock(&queue->lock);

  return dequeued_node;
}

/**
 * free_url_queue - Deallocates memory of queue and its nodes
 * @queue: Pointer to queue that will be freed
 */
void free_url_queue(url_queue* queue) {
  // Free each remaining node in queue
  while (!isEmpty(queue)) {
    url_queue_node* dequeued_data = dequeue_url(queue);
    free(dequeued_data->url);
    free(dequeued_data);
  }

  free(queue);
  return;
}

/************************************
 * OCCURRENCE REPORTS
 ************************************/

/**
 * init_occurrence_report - Initializes occurrence_report struct
 * @word : important words array
 * @return: the occurrence report
 */
occurrence_report* init_occurrence_report() {
  word* words = get_impwords();

  // Initialize a blank report
  occurrence_report* report =
      (occurrence_report*)malloc(sizeof(occurrence_report));
  report->word_counts = NULL;
  // We don't want a null report, so we can check if a report is
  // null and catch an error that way.

  if (pthread_mutex_init(&report->lock, NULL) != 0) {
    // Mutex failed to initialize, return it empty
    return report;
  }

  // Count for malloc
  int count = 0;
  while (words[count] != NULL && words[count][0] != '\0') {
    count++;
  }

  // malloc the tracked words by count. note delimiter
  report->word_counts = malloc(sizeof(word_count) * (count + 1));
  if (report->word_counts == NULL) {
    // malloc failed
    return report;
  }

  // Populate malloc with word and 0 count
  int failure = -1;  // track presence of failure and where
  for (int i = 0; i < count; i++) {
    report->word_counts[i].word = strdup(words[i]);
    if (report->word_counts[i].word == NULL) {
      // strdup failed
      failure = i;
      break;
    }
    report->word_counts[i].count = 0;
  }

  // If strdup failed, free successful allocations up to fail
  // and return empty report.
  if (failure != -1) {
    for (int k = 0; k < failure; k++) {
      // freeing the words
      free(report->word_counts[k].word);
    }
    // free array, return empty report
    free(report->word_counts);
    report->word_counts = NULL;
    free(report);
    free(words);
    return NULL;
  }

  // Delimiter for the word_counts in the report.
  report->word_counts[count].word = NULL;
  report->word_counts[count].count = 0;  // Not needed but to avoid garbage

  free(words);
  return report;
}

/**
 * update_occurrence_report - update the global report with input from a local
 * one
 * @globalor: the global occurrence report
 * @localor: the individual occurrence report a thread is currently handling
 * @return: 0 if successful, -1 otherwise
 */
int update_occurrence_report(occurrence_report* globalor,
                             occurrence_report* localor) {
  // Lock global report, check for errors
  if (pthread_mutex_lock(&globalor->lock) != 0) {
    // failed to lock
    return -1;
  }

  // Check if any reports returned NULL
  if (globalor->word_counts == NULL || localor->word_counts == NULL) {
    pthread_mutex_unlock(&globalor->lock);
    return -1;
  }

  // All reports should track all impwords in the same order.
  int i = 0;
  while (globalor->word_counts[i].word != NULL) {
    globalor->word_counts[i].count += localor->word_counts[i].count;
    i++;
  }

  // Release the thread locks.
  pthread_mutex_unlock(&globalor->lock);

  return 0;  // success
}

/**
 * free_occurrence_report - frees the occurrence report struct
 * @or: the report to be freed
 */
void free_occurrence_report(occurrence_report* or) {
  // Free each word first
  int i = 0;
  while (or->word_counts[i].word != NULL) {
    free(or->word_counts[i].word);
    i++;
  }

  // Free the word counts array
  free(or->word_counts);

  // Destroy the lock
  pthread_mutex_destroy(&or->lock);

  // Free or
  free(or);
}

/**
 * count_occurrences - processes html, counts words
 * @html: the stuff to process
 * @return: the occurrence report with updated counts
 */
occurrence_report* count_occurrences(char* html) {
  // Initialize an occurrence report with the important words
  occurrence_report* report = init_occurrence_report();

  if (report->word_counts == NULL || html == NULL) {
    printf("Error: bad report or html");
    // TODO: FURTHER ERROR HANDLING
    fprintf(stderr, "Error: report or html is null");
    exit(EXIT_FAILURE);
  }

  // strtok_r for thread safe string tokenizer
  char* current;  // current position in the html

  // some delimiters but it's ok not to be super exact
  char* delimiters = ".,-\"';:?!@#/&*()[]{}<>/\\_~+= \t\r\n\f\v";

  // iterate through important words
  for (int i = 0; report->word_counts[i].word != NULL; i++) {
    char* copy = strdup(html);

    // first token
    char* token = strtok_r(copy, delimiters, &current);

    while (token != NULL) {
      // if we get a hit
      printf("current:%s\n\n", &html[token - copy]);
      if (startswith(&html[token - copy], report->word_counts[i].word)) {
        report->word_counts[i].count++;
      }

      // next token
      token = strtok_r(current, delimiters, &current);
    }

    free(copy);
  }

  return report;
}

void log_occurrence_report(occurrence_report* or) {
  word_count* wc = or->word_counts;
  while (wc != NULL && wc->word != NULL && wc->word[0] != '\0') {
    printf("%s %d\n", wc->word, wc->count);
    wc++;
  }
}

/***********************************************************/

/*************************************************************
 * IMPWORDS (TODO)
 ************************************************************/
void init_impwords_file() {}

word* get_impwords() {
  // Read important words
  char* impwords_content = read_file(impwords_file);
  if (!impwords_content) {
    fprintf(stderr, "Error: Could not load important words\n");
    exit(EXIT_FAILURE);  // TODO: return empty occurrence_report
  }

  // Split important words into array
  word* important_words_array =
      (word*)malloc(26 * sizeof(word));  // assume max 25 words
  int important_word_count = 0;
  // splits important words separated by '\n'
  char* token = strtok(impwords_content, "\n");
  while (token != NULL) {
    // strdup(token) makes a copy of the word and store in imp_words_array
    word word = token;
    word[strcspn(word, "\n")] = 0;
    word[strcspn(word, "\r\n")] = 0;
    important_words_array[important_word_count++] = strdup(word);
    token = strtok(NULL, "\n");
    // After this our array looks like this, e.g: [0]: "data", [1]: "science",
    // etc.
  }

  important_words_array[important_word_count] = NULL;

  free(impwords_content);
  return important_words_array;
}

/*************************************************************
 * THREADS
 ************************************************************/
typedef struct thread_args_st {
  url_queue* queue;
  occurrence_report* globalor;
} thread_args;

void* thread_worker(void* args) {
  thread_args* threadargs = (thread_args*)args;

  while (true) {
    url_queue_node* node = dequeue_url(threadargs->queue);
    if (node == NULL)
      break;

    char filename[20];
    sprintf(filename, "page%d.html", node->index);

    char* html = fetch(node->url);
    write_file(filename, html);
    occurrence_report* localor = count_occurrences(html);
    update_occurrence_report(threadargs->globalor, localor);
    free(node);
  }
}

/*************************************************************
 * MAIN METHOD
 ************************************************************/
int main(int argc, char* argv[]) {
  printf("=== RUC Web Crawler ===\n");

  // Initialize queue
  url_queue* queue = init_url_queue();

  // Read input file and keep populating the queues
  char* urls = read_file(urls_file);
  char* url = strtok(urls, "\n");
  while (url != NULL) {
    url[strcspn(url, "\n")] = 0;
    url[strcspn(url, "\r\n")] = 0;
    enqueue_url(queue, url);
    url = strtok(NULL, "\n");
  }

  // Make pthreads, start them and join to wait
  thread_args threadargs = {
      .queue = queue,
      .globalor = init_occurrence_report(),
  };
  pthread_t threads[threads_count];
  for (int i = 0; i < threads_count; i++) {
    pthread_create(&threads[i], NULL, thread_worker, (void*)&threadargs);
  }
  for (int i = 0; i < threads_count; i++) {
    pthread_join(threads[i], NULL);
  }

  // Log occurrence report
  log_occurrence_report(threadargs.globalor);

  // Free EVERYTHING (make sure no memory leaks!)
  free(urls);
  free_url_queue(threadargs.queue);
  free_occurrence_report(threadargs.globalor);

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

  url_queue_node* pop1 = dequeue_url(queue);
  printf("Popped String is: %s\n", pop1->url);
  free(pop1->url);
  free(pop1);
  printf("Size is: %d\n", queue->size);

  if (isEmpty(queue)) {
    printf("Queue is empty\n");
  } else {
    printf("Queue is NOT empty yet\n");
  }

  printf("Freeing the rest of the queue...\n");
  free_url_queue(queue);
}

void test_read_file() {
  // === TESTING read_file() === //
  char* file_data = read_file(urls_file);
  if (file_data) {
    printf("\n--- Contents of %s ---\n", urls_file);
    printf("%s\n", file_data);
    free(file_data);  // free memory after use
  } else {
    printf("Failed to read file: %s\n", urls_file);
  }
}

void test_fetch() {
  // === TESTING fetch() === //
  url test_url =
      "https://google.com";  // we will replace this with URL from 'urls.txt'
  char* html_data = fetch(test_url);
  if (html_data) {
    printf("\n--- Fetched HTML from %s ---\n", test_url);
    printf("%.1000s\n", html_data);  // only print first 500 chars for testing
    free(html_data);                 // free memory after use
  } else {
    printf("Failed to fetch URL: %s\n", test_url);
  }
}
