/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
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

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

void init_words(word_count_list_t* wclist) {
  pthread_mutex_init(&wclist->lock, NULL);
  list_init(&wclist->lst);
}

size_t len_words(word_count_list_t* wclist) {
  int len = 0;
  pthread_mutex_lock(&wclist->lock);
  len = list_size(&wclist->lst);
  pthread_mutex_unlock(&wclist->lock);
  return len;
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  word_count_t* wc = NULL;
  struct list_elem* e;
  pthread_mutex_lock(&wclist->lock);
  for (e = list_begin(&wclist->lst); e != list_end(&wclist->lst); e = list_next(e)) {
    wc = list_entry(e, word_count_t, elem);
    if (strcmp(wc->word, word) == 0) {
      pthread_mutex_unlock(&wclist->lock);
      return wc;
    }
  }

  pthread_mutex_unlock(&wclist->lock);
  return NULL;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  word_count_t* wc = find_word(wclist, word);
  
  if (wc != NULL) {
    wc->count += 1;
    return wc;
  }

  wc = (word_count_t*) malloc(sizeof(word_count_t));
  wc->word = strcpy(malloc(strlen(word)), word);
  wc->count = 1;
  wc->elem.prev = NULL;
  wc->elem.next = NULL;
  
  pthread_mutex_lock(&wclist->lock);
  list_push_front(&wclist->lst, &wc->elem);
  pthread_mutex_unlock(&wclist->lock);
  
  return wc;
}

void add_word_counts(word_count_list_t* dest, word_count_list_t* src) {

  word_count_t* wc;
  word_count_t* target;
  struct list_elem* e;
  for (e = list_begin(&src->lst); e != list_end(&src->lst); e = list_next(e)) {
    wc = list_entry(e, word_count_t, elem);
    target = add_word(dest, wc->word);
    target->count -= 1;
    target->count += wc->count;
  }

}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  word_count_t* wc = NULL;
  struct list_elem* e;
  // pthread_mutex_lock(&wclist->lock);

  for (e = list_begin(&wclist->lst); e != list_end(&wclist->lst); e = list_next(e)) {
    wc = list_entry(e, word_count_t, elem);
    fprintf(outfile, "%s %d\n", wc->word, wc->count);
  }

  // pthread_mutex_unlock(&wclist->lock);
}

static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {
  word_count_t* wc1 = list_entry(ewc1, word_count_t, elem);
  word_count_t* wc2 = list_entry(ewc2, word_count_t, elem);
  int cmp = strcmp(wc1->word, wc2->word);
  if (cmp < 0)
    return true;
  return false;
}


void wordcount_sort(word_count_list_t* wclist,
    bool less(const word_count_t*, const word_count_t*)) {
  // pthread_mutex_lock(&wclist->lock);
  list_sort(&wclist->lst, less_list, less);
  // pthread_mutex_unlock(&wclist->lock);
}
