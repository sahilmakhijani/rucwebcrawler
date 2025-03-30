#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Config (Global Variables)
// 1. input_file = urls.txt
// 2. output_directory = current_directory
// 3. important_words = hardcoded_array (e.g., data, science, algorithm)
// 4. no_of_threads = 10

// Queue Data Structure with Mutex Lock for Shared Memory (Multi-threading)

// Note: Handle file, url and networks errors!!
// 1. Read input file
// 2. Fetch html from url using libcurl
// 3. Write content to new file
// 4. Count occurances in content

// Thread Worker Function

int main(int argc, char* argv[]) {
  printf("=== RUC Web Crawler ===\n");

  // Read input file
  // Initialize queue
  // Make pthreads, start them and join to wait
  // Free EVERYTHING (make sure no memory leaks!)

  return EXIT_SUCCESS;
}
