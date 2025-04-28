#include <ctype.h>
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
 * FILE OPS
 ************************************************************/

// Memory struct
typedef struct MemoryStruct {
  char* memory;
  size_t size;
} MemoryStruct;

// Function to read a file and return its contents
content read_file(char* filename) {
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
  content buffer = (content)malloc(filesize + 1);
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
content fetch(url url_to_fetch) {
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
  curl_easy_setopt(curl_handle, CURLOPT_URL, url_to_fetch);
  // this tells curl to use our WriteMemCallback to store new data
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  // gives curl a pointer to chunk to write new data
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);

  // curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "rucwebcrawler/1.0");

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
int write_file(char* filename, content data) {
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

// for case-insensitive comparison
void to_lower(char* str) {
  for (int i = 0; str[i]; i++) {
    str[i] = tolower(str[i]);
  }
}

occurrence_report count_occurrences(content html) {
  // Read important words
  content imp_words_file = read_file(important_words);
  if (!imp_words_file) {
    fprintf(stderr, "Error: Could not load important words\n");
    exit(EXIT_FAILURE);  // or return empty occurrence_report
  }

  // Split important words into array
  char* important_words_array[25];  // assume max 25 words
  int important_word_count = 0;
  // splits important words separated by '\n'
  char* token = strtok(imp_words_file, "\n");
  while (token != NULL) {
    // strdup(token) makes a copy of the word and store in imp_words_array
    important_words_array[important_word_count++] = strdup(token);
    token = strtok(NULL, "\n");
    // After this our array looks like this, e.g: [0]: "data", [1]: "science",
    // etc.
  }

  // Initialize occurrence report
  occurrence_report report;
  // allocate memory for an array of word_count struct
  // each word_count has: the imp word, count (# of times it appeared)
  report.word_counts = malloc(sizeof(word_count) * important_word_count);
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