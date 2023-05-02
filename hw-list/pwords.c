/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>

#include "word_count.h"
#include "word_helpers.h"

struct threadinfo {
  word_count_list_t* wclist;
  char* filename;
};

void* threadfun(void* args) {
  struct threadinfo* info = (struct threadinfo*) args;
  FILE* f = fopen(info->filename, "r");
  if (f == NULL) {
    printf("ERROR: fail open file %s\n", info->filename);
    pthread_exit(NULL);
  }
  word_count_list_t thread_counts;
  init_words(&thread_counts);
  count_words(&thread_counts, f);
  
  fclose(f);
  add_word_counts(info->wclist, &thread_counts);
  pthread_exit(NULL);
}

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char* argv[]) {
  /* Create the empty data structure. */
  word_count_list_t word_counts;
  init_words(&word_counts);

  if (argc <= 1) {
    /* Process stdin in a single thread. */
    count_words(&word_counts, stdin);
  } else {
    int rc;
    int nthreads = argc - 1;
    pthread_t threads[nthreads];
    struct threadinfo* info = malloc(sizeof(struct threadinfo));
    for (int t = 1; t < argc; t++) {
      char* fn = malloc(strlen(argv[t]));
      strcpy(fn, argv[t]);
      printf("[thread %d], filename=%s\n", t, fn);
      info->filename = fn;
      info->wclist = &word_counts;
      rc = pthread_create(&threads[t-1], NULL, threadfun, (void*)info);
      if (rc) {
        printf("ERROR, return code from pthread_create() is %d\n", rc);
      }
    }

    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    void* done;
    for (int i = 0; i < nthreads; i++) {
      pthread_mutex_lock(&lock);
      pthread_join(threads[i], &done);
      pthread_mutex_unlock(&lock);
    }
  }

  /* Output final result of all threads' work. */
  wordcount_sort(&word_counts, less_count);
  fprint_words(&word_counts, stdout);
  pthread_exit(NULL);
}
