#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

typedef char* url;
typedef char* content;
content read_file(char* filename);
content fetch(url url);

occurrence_report count_occurrences(content html);
int write_file(char* filename, content);

void init_impwords_file();  // (e.g., data, science, algorithm)

void* thread_worker(void* args);  // args[0] = queue and args[1] = globalor



// Implementation
// TODO

// Implementation
// TODO

/** FILE OPS */
// Memory struct 
typedef struct MemoryStruct {
  char* memory;  // pointer to hold html data
  size_t size;
} MemoryStruct;

// Function to read a file and return its contents
content read_file(char* filename) {
  FILE* file = fopen(filename, "r");
  if (!file) {
    // error handling FNF / wrong file name
    fprintf(stderr, "Error: Could not open file %s\n", filename);
    return NULL;
  }

  // to get the size of the file
  fseek(file, 0, SEEK_END);
  long filesize = ftell(file); //ftell gets the current position (size)
  rewind(file); // takes the pointer back to the top of file

  // error handling for empty file
  if (filesize <= 0) {
    fprintf(stderr, "Error: File %s is empty or invalid\n", filename);
    fclose(file);
    return NULL;
  }

  // dynamically allocate memory
  content buffer = (content)malloc(filesize + 1); // 1 extra byte for '\0' terminator
  if (!buffer) {
    // memory error handling
    fprintf(stderr, "Error: Memory allocation failed for file %s\n", filename);
    fclose(file);
    return NULL;
  }

  // reads the entire file into buffer
  size_t read_size = fread(buffer, 1, filesize, file);
  if (read_size != filesize) {
    // error handling if read fails
    fprintf(stderr, "Error: Failed to read complete file %s\n", filename);
    free(buffer); // free memory
    fclose(file);
    return NULL;
  }

  buffer[filesize] = '\0'; // add null terminator at the end of buffer

  fclose(file); // close file
  return buffer; // return char content
}

// Callback function for libcurl 
// Helps curl store downloaded data into memory
static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t real_size = size * nmemb; // to get the # of bytes
  // cast back userp to MemoryStruct*
  struct MemoryStruct* mem = (struct MemoryStruct*)userp;

  // reallocate the buffer to hold new + previous data
  char* ptr = realloc(mem->memory, mem->size + real_size + 1);
  if (!ptr) {
    // memory error handling
    fprintf(stderr, "Error: Not enough memory (realloc returned NULL)\n");
    return 0;
  }

  // copies the data into right position inside the buffer
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, real_size);
  // update the size of buffer
  mem->size += real_size;
  mem->memory[mem->size] = '\0'; // null terminate
  return real_size; // return the # of bytes we read
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
  if (!curl_handle) {
    // error handling if curl failed to initialize
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
  if (res != CURLE_OK) {
    // if something goes wrong, we free the memory and cleanup
    fprintf(stderr, "Error: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    free(chunk.memory);
    curl_easy_cleanup(curl_handle); 
    return NULL;
  }

  curl_easy_cleanup(curl_handle);

  return chunk.memory; // returns the downloaded HTML content
}

// WRITE FUNCTION
// takes in char content in the memory
int write_file(char* filename, content data) {
  FILE* file = fopen(filename, "w"); 
  if (!file) {
    // error handling 
    fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
    return -1;
  }

  // gets the length of data we want to write
  size_t data_length = strlen(data);
  // size of each item is 1 bytes, how many items to write, and file is where to write
  size_t written = fwrite(data, 1, data_length, file);
  if (written != data_length) {
    fprintf(stderr, "Error: Failed to write complete data to %s\n", filename);
    fclose(file);
    return -1;
  }

  fclose(file);
  return 0; // success
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
    fprintf(stderr, "Error: Could not load important words.\n");
    exit(EXIT_FAILURE); // or return empty occurrence_report
  }

  // Split important words into array
  char* important_words_array[25]; // assume max 25 words
  int important_word_count = 0;
  // splits important words separated by '\n'
  char* token = strtok(imp_words_file, "\n");
  while (token != NULL) {
    // strdup(token) makes a copy of the word and store in imp_words_array
    important_words_array[important_word_count++] = strdup(token);
    token = strtok(NULL, "\n");
    // After this our array looks like this, e.g: [0]: "data", [1]: "science", etc.
  }


  // Initialize occurrence report
  occurrence_report report;
  // allocate memory for an array of word_count struct
  // each word_count has: the imp word, count (# of times it appeared)
  report.word_counts = malloc(sizeof(word_count) * important_word_count);
  // initialize lock to protect (because multithreading)
  pthread_mutex_init(&(report.lock), NULL);

  // for each imp_word: set its word name, set count to 0
  for(int i=0; i<important_word_count; i++) {
    report.word_counts[i].word = important_words_array[i];
    report.word_counts[i].count = 0;

  }

  // Tokenize html content into words

  // make a copy of html string because strtok modifies the org string
  char* html_copy = strdup(html); 
  // split on common html separators, these chars are not part of words
  char* word = strtok(html_copy, " \n\t<>/=\"'-_.:,;!?");
  while (word != NULL) { // loop thru each word
    char lower_word[256]; // initialize local buffer
    // copy the word into buffer, lower_word.
    strncpy(lower_word, word, sizeof(lower_word) - 1);
    lower_word[sizeof(lower_word) - 1] = '\0'; // null terminate the buffer
    to_lowercase(lower_word); // convert to lower case

    // Compare to important words
    for (int i = 0; i < important_word_count; i++) {
      if (strcmp(lower_word, important_words_array[i]) == 0) {
      // if the word matches to imp_word, lock the mutex (to avoid thread race conditions)
      // need to lock to avoid multiple threads updating the same word
        pthread_mutex_lock(&(report.lock));
        report.word_counts[i].count++;    // increment count
        pthread_mutex_unlock(&(report.lock));  // unlock mutex
      }
    }

    // get the next word from html
    word = strtok(NULL, " \n\t<>/=\"'-_.:,;!?");
  }

  // free memory
  free(html_copy);
  free(imp_words_file);

  return report;  // return the filled report
}



// -- MAIN FUNCTION -- //

int main(int argc, char* argv[]) {
  printf("=== RUC Web Crawler ===\n");

  // Initialize queue
  // Read input file and keep populating the queues

  // === TESTING read_file() === //
  content file_data = read_file(input_file);
  if (file_data) {
    printf("\n--- Contents of %s ---\n", input_file);
    printf("%s\n", file_data);
    free(file_data); // free memory after use
  } else {
    printf("Failed to read file: %s\n", input_file);
  }

  // === TESTING fetch() === //
  url test_url = "https://google.com"; // we will replace this with URL from 'urls.txt' 
  content html_data = fetch(test_url);
  if (html_data) {
    printf("\n--- Fetched HTML from %s ---\n", test_url);
    printf("%.1000s\n", html_data); // only print first 500 chars for testing
    free(html_data); // free memory after use
  } else { 
    printf("Failed to fetch URL: %s\n", test_url);
  }



  // Make pthreads, start them and join to wait
  // Free EVERYTHING (make sure no memory leaks!)

  return EXIT_SUCCESS;
}
