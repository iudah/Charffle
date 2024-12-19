#include <stdint.h>
#include <sys/types.h>
#ifndef DEFAULT_WORDS_DIRECTORY
#define DEFAULT_WORDS_DIRECTORY ""
#endif

#include <bits/pthread_types.h>
#include <ctype.h>
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int active_thread_count = 0;
static char *words_directory_path = NULL;
char error_buffer[2048];

#define update_thread_count(x)                                                 \
  pthread_mutex_lock(&thread_count_mutex);                                     \
  active_thread_count x;                                                       \
  pthread_mutex_unlock(&thread_count_mutex);

struct anagram_context {
  FILE *dictionary_file;
  char *sorted_word;
  int *unique_letter_indices;
  char **possible_anagrams;
  uint64_t num_anagrams_found;
  uint64_t anagrams_capacity;
  uint64_t input_word_length;
  uint64_t thread_index;
  char unique_letter;
};

struct anagram_stack {
  char *current_word;
  char *current_candidate;
  uint64_t current_position, candidate_index, loop_index, word_length;
};

_Thread_local static struct anagram_stack *stack = NULL;
_Thread_local static uint64_t stack_len = 0;
_Thread_local static uint64_t stack_top = 0;
_Thread_local static uint64_t stack_limit = 10;

char *copy_non_interest_anagram_candidates(
    const char *original_candidates, const uint64_t original_candidates_length,
    char *filtered_candidates, uint64_t excluded_index) {
  uint64_t j = 0;
  for (; j < excluded_index; j++) {
    filtered_candidates[j] = original_candidates[j];
  }
  // exclude candidate of interest
  for (uint64_t k = excluded_index + 1; k < original_candidates_length; k++) {
    filtered_candidates[j++] = original_candidates[k];
  }
  return filtered_candidates;
}

int append_word(char ***word_list, uint64_t current_index, uint64_t *capacity,
                char *new_word, uint64_t new_word_length) {
  if (*capacity <= current_index) {
    void *tmp =
        realloc(*word_list, sizeof(char *) * (*capacity += *capacity / 2));
    if (!tmp)
      return -1;
    *word_list = tmp;
  }

  if (new_word_length) {
    (*word_list)[current_index] = malloc((new_word_length + 1) * sizeof(char));
    memcpy((*word_list)[current_index], new_word, new_word_length);
  } else {
    (*word_list)[current_index] = new_word;
  }

  return 0;
}

char has_match(char **word_list, char *current_word,
               uint64_t current_word_length, volatile uint64_t *current_index,
               uint64_t list_end_index) {
  if (*current_index == list_end_index)
    return 0;
  volatile int cmp;
  while ((cmp = strncmp(word_list[*current_index], current_word,
                        current_word_length))) {
    if ((*current_index += 1) >= list_end_index || cmp > 0) {
      return 0;
    };
  }
  return 1;
}

int push_to_stack(const char *word, uint64_t word_len, uint64_t k, uint64_t j,
                  uint64_t l, const char *candidates, uint64_t candidates_len,
                  uint64_t max_len) {
  if (!stack) {
    stack = calloc(stack_limit, sizeof(*stack));
  }

  uint64_t i = 0;

  if (!stack_len) {
    i = 0;
    stack_len += 1;
  } else {
    i = stack_top += 1;

    if (stack_top == stack_len) {
      stack_len += 1;
    }

#define LIMIT 10

    if (stack_top == stack_limit) {
      stack_limit += LIMIT;
      void *tmp = realloc(stack, stack_limit * sizeof(*stack));
      if (!tmp) {
        printf("could not allocation memory for stack");
        return 0;
      }

      stack = tmp;

      for (uint64_t i = stack_top; i < stack_limit; i++) {
        stack[i].current_candidate = stack[i].current_word = NULL;
      }
    }
  }

  stack[i].loop_index = l;
  stack[i].candidate_index = j;
  stack[i].current_position = k;
  stack[i].word_length = word_len;
  if (!stack[i].current_word)
    stack[i].current_word = malloc(max_len + 1);
  if (!stack[i].current_candidate)
    stack[i].current_candidate = malloc(candidates_len + 1);

  memcpy(stack[i].current_word, word, max_len);
  memcpy(stack[i].current_candidate, candidates, candidates_len);

  return 1;
}

void pop_from_stack(char *word, uint64_t *word_len, uint64_t *k, uint64_t *j,
                    uint64_t *l, char *candidates, uint64_t candidates_len,
                    uint64_t max_len) {
  if (!stack_len)
    return;

  uint64_t i = stack_top;
  if (stack_top)
    stack_top--;
  else
    stack_len = 0;

  memcpy(candidates, stack[i].current_candidate, candidates_len);
  memcpy(word, stack[i].current_word, max_len);
  *word_len = stack[i].word_length;
  *k = stack[i].current_position;
  *j = stack[i].candidate_index;
  *l = stack[i].loop_index;
}

void *do_anagram(struct anagram_context *anagram_data) {
  // initial letter
  char word[anagram_data->input_word_length + 1];
  word[0] = anagram_data->unique_letter;
  word[1] = 0;

  // remove initial letter
  uint64_t candidate_length = anagram_data->input_word_length - 1;
  char new_anagram_candidates[candidate_length + 1];
  copy_non_interest_anagram_candidates(
      anagram_data->sorted_word, anagram_data->input_word_length,
      new_anagram_candidates,
      anagram_data->unique_letter_indices[anagram_data->thread_index]);
  new_anagram_candidates[candidate_length] = 0;

  // load words
  uint64_t tops[anagram_data->input_word_length];
  uint64_t bottoms[anagram_data->input_word_length];

  memset(tops, 0, sizeof(tops));
  memset(bottoms, 0, sizeof(bottoms));

  char buffer[1024];
  int64_t i = 0;
  uint64_t limit = 100;
  uint64_t idx = 0;
  uint64_t len = 0;
  char within_match = 0;
  char **words = malloc(limit * sizeof(*words));
  while (fgets(buffer, sizeof(buffer), anagram_data->dictionary_file)) {
    if (buffer[1] == new_anagram_candidates[i]) {
      if (!within_match) {
        within_match = 1;
        tops[i] = idx;
      }
      len = strlen(buffer) - 1;
      buffer[len] = 0;
      if (append_word(&words, idx++, &limit, buffer, len)) {
        printf("Could not append new word");
        return NULL;
      }
    } else {
      if (within_match) {
        within_match = 0;
        bottoms[i] = idx - 1;
        while (new_anagram_candidates[i] == new_anagram_candidates[i + 1]) {
          i++;
        }
        i++;

        if (i >= (int64_t)(anagram_data->input_word_length - 1))
          break;
      }
    }
  }

  // do perm
  printf("%s|%" PRIu64 "|%s\n", word, anagram_data->thread_index,
         new_anagram_candidates);

  uint64_t bottom = idx;
  idx = 0;

  uint64_t l = 0;
  char candidates[candidate_length + 1];
  memcpy(candidates, new_anagram_candidates, sizeof(candidates));
  // while (l < candidate_length) {
  uint64_t anagram_len = 2;
  uint64_t k = 1;
  uint64_t j = 0;
  while (j < candidate_length) {
    word[k] = candidates[j];
    word[k + 1] = 0;

    putchar('-');
    puts(word);

    if (has_match(words, word, anagram_len, &idx, bottom)) {
      putchar(' ');
      puts(words[idx]);

      if (l < j)
        l = j;
      l++;
      if (l < candidate_length) {
        push_to_stack(word, anagram_len, k, j, l, candidates, candidate_length,
                      anagram_data->input_word_length);
      }
      if (anagram_len > 2 && words[idx][anagram_len] == 0) {
        append_word(&anagram_data->possible_anagrams,
                    anagram_data->num_anagrams_found,
                    &anagram_data->anagrams_capacity, words[idx], 0);
        anagram_data->num_anagrams_found++;
        idx++;
      }

      anagram_len++;
      k++;
    }
    j++;

    if (j == candidate_length && stack_len) {
      pop_from_stack(word, &anagram_len, &k, &j, &l, candidates,
                     candidate_length, anagram_data->input_word_length);

      char tmp = candidates[l];
      candidates[l] = candidates[j];
      candidates[j] = tmp;
    }
  }
  //}

  update_thread_count(--);

  return NULL;
}

void sort(char *word, uint64_t word_len) {
  uint64_t i = 1;
  while (i < word_len) {
    while (word[i - 1] <= word[i]) {
      i++;
    }
    if (i < word_len) {
      while ((i > 0) && (word[i - 1] > word[i])) {
        char tmp = word[i];
        word[i] = word[i - 1];
        word[i - 1] = tmp;
        i--;
      }
      if (!i)
        i = 1;
    }
  }
}

void unique(char *unik, int *idxs, uint64_t word_len) {
  uint64_t i = 1;
  uint64_t j = 1;
  while (i < word_len) {
    while (unik[i - 1] == unik[i]) {
      i++;
    }
    idxs[j] = i;
    unik[j++] = unik[i++];
  }
  unik[j] = 0;
}

char **anagram(const char *word, uint64_t *len) {
  uint64_t word_len = strlen(word);

  char upper[word_len + 1]; // = malloc(word_len + 1);
  memcpy(upper, word, word_len + 1);

  char *uc = upper;
  while (*uc) {
    *uc = tolower(*uc);
    uc++;
  }

  sort(upper, word_len);

  char unik[word_len + 1];
  memcpy(unik, upper, word_len + 1);
  int idxs[word_len + 1];
  idxs[0] = 0;
  unique(unik, idxs, word_len);
  uint64_t uniq_len = strlen(unik);

  struct anagram_context anagram_data[uniq_len];
  pthread_t anagram_threads[uniq_len];

  for (uint64_t i = 0; i < uniq_len; i++) {
    char fpath[256];
    snprintf(fpath, sizeof(fpath), "%s/words/%c_words.txt",
             words_directory_path ? words_directory_path
                                  : DEFAULT_WORDS_DIRECTORY,
             unik[i]);

    anagram_data[i].dictionary_file = fopen(fpath, "r");
    anagram_data[i].sorted_word = upper;
    anagram_data[i].unique_letter_indices = idxs;
    anagram_data[i].anagrams_capacity = 20;
    anagram_data[i].possible_anagrams =
        malloc(anagram_data[i].anagrams_capacity * sizeof(char *));
    anagram_data[i].num_anagrams_found = 0;
    anagram_data[i].input_word_length = word_len;
    anagram_data[i].thread_index = i;
    anagram_data[i].unique_letter = unik[i];

    update_thread_count(++);
    if (pthread_create(anagram_threads + i, NULL, (void *)do_anagram,
                       anagram_data + i)) {
      update_thread_count(--);
    }
  }

  while (1) {
    sleep(1);
    if (active_thread_count <= 0)
      break;
  }
  for (uint64_t i = 0; i < uniq_len; i++) {
    pthread_join(anagram_threads[i], NULL);
    fclose(anagram_data[i].dictionary_file);
  }

  // move results into one list
  for (int64_t i = 1; i < (int64_t)uniq_len; i++) {

    if (anagram_data[i].num_anagrams_found > 0) {

      if (anagram_data[0].anagrams_capacity <
          (anagram_data[0].num_anagrams_found +
           anagram_data[i].num_anagrams_found)) {
        anagram_data[0].anagrams_capacity +=
            anagram_data[i].num_anagrams_found * (uniq_len - i);

        void *tmp = realloc(anagram_data[0].possible_anagrams,
                            sizeof(anagram_data[0].possible_anagrams[0]) *
                                anagram_data[0].anagrams_capacity);
        if (!tmp) {
          printf("Could not allocate memory for anagrams");
          return NULL;
        }

        anagram_data[0].possible_anagrams = tmp;
      }

      if (anagram_data[i].num_anagrams_found > 100) {
        memmove(anagram_data[0].possible_anagrams +
                    anagram_data[0].num_anagrams_found,
                anagram_data[i].possible_anagrams,
                sizeof(anagram_data[0].possible_anagrams[0]) *
                    anagram_data[i].num_anagrams_found);

        anagram_data[0].num_anagrams_found +=
            anagram_data[i].num_anagrams_found;
      } else {
        char **new_location = anagram_data[0].possible_anagrams +
                              anagram_data[0].num_anagrams_found;

        anagram_data[0].num_anagrams_found +=
            anagram_data[i].num_anagrams_found;

        while (anagram_data[i].num_anagrams_found) {
#define IDX anagram_data[i].num_anagrams_found

          --IDX;
          new_location[IDX] = anagram_data[i].possible_anagrams[IDX];
#undef IDX
        }
      }
    }
    free(anagram_data[i].possible_anagrams);
  }

  // return result
  *len = anagram_data[0].anagrams_capacity;
  return anagram_data[0].possible_anagrams;
}

char set_words_directory_path(const char *path) {
  if (!path)
    return 0;
  if (words_directory_path)
    free(words_directory_path);
  uint64_t pathlen = strlen(path);
  words_directory_path = malloc(pathlen);
  memcpy(words_directory_path, path, pathlen);
  return 1;
}

__attribute__((destructor)) void release_word_path() {
  free(words_directory_path);
}

#ifdef ZO
int main() {
  // FILE *english_words = fopen("english-words-master/");
  char *word = "sally";
  uint64_t len;

  char **a = anagram(word, &len);
  // for (int i = 0; i < len; i++) {
  //   puts(a[i]);
  // }

  printf("%" PRIu64 " \n", len);
}
#endif