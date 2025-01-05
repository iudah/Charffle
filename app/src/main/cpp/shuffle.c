#ifndef DEFAULT_WORDS_DIRECTORY
#define DEFAULT_WORDS_DIRECTORY "."
#endif

#include <bits/pthread_types.h>
#include <ctype.h>
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int active_thread_count = 0;
static char *words_directory_path = NULL;
char error_buffer[2048];

static inline void update_thread_count(int x) {
  pthread_mutex_lock(&thread_count_mutex);
  active_thread_count += x;
  pthread_mutex_unlock(&thread_count_mutex);
}

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
  printf("Copying anagram candidates excluding index %" PRIu64 "\n",
         excluded_index);
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
    printf("Increasing word list capacity from %" PRIu64 "\n", *capacity);
    void *tmp =
        realloc(*word_list, sizeof(char *) * (*capacity += *capacity / 2));
    if (!tmp) {
      printf("Failed to allocate memory for word list\n");
      return -1;
    }
    *word_list = tmp;
  }

  if (new_word_length) {
    (*word_list)[current_index] = malloc((new_word_length + 1) * sizeof(char));
    memcpy((*word_list)[current_index], new_word, new_word_length + 1);
  } else {
    (*word_list)[current_index] = new_word;
  }

  printf("Appended word '%s' at index %" PRIu64 "\n", new_word, current_index);
  return 0;
}

char has_match(char **word_list, char *current_word,
               uint64_t current_word_length, volatile uint64_t *current_index,
               uint64_t list_end_index) {
  printf("Checking match for word '%s' in word list\n", current_word);
  if (*current_index == list_end_index) {
    printf("No match found. Current index is at end of list.\n");
    return 0;
  }
  volatile int cmp;
  while ((cmp = strncmp(word_list[*current_index], current_word,
                        current_word_length))) {

    if (*current_index == 338) {
      printf("...\n");
    }
    if (cmp < 0) {
      if (word_list[*current_index])
        free(word_list[*current_index]);
      word_list[*current_index] = NULL;
      *current_index += 1;
    }
    if ((*current_index) >= list_end_index || cmp > 0) {
      printf("No match found after comparison.\n");
      return 0;
    };
  }
  printf("Match found for '%s' at %" PRIu64 "\n", current_word, *current_index);
  return 1;
}

int push_to_stack(const char *word, uint64_t word_len, uint64_t k, uint64_t j,
                  uint64_t l, const char *candidates, uint64_t candidates_len,
                  uint64_t max_len) {
  uint64_t i;

  if (!stack) {
    printf("Initializing stack\n");
    stack = calloc(stack_limit, sizeof(*stack));
    i = stack_top = 0;
    stack_len = 1;
  } else {
    i = ++stack_top;
  }
  if (stack_top >= stack_len) {
    printf("Extending stack\n");
    stack_len = stack_top + 1;
  }

  if (stack_top >= stack_limit) {
    printf("Increasing stack limit\n");
    stack_limit += 10;
    void *tmp = realloc(stack, stack_limit * sizeof(*stack));
    if (!tmp) {
      printf("Could not allocate memory for stack\n");
      return 0;
    }

    stack = tmp;

    for (uint64_t i = stack_top; i < stack_limit; i++) {
      stack[i].current_candidate = stack[i].current_word = NULL;
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

  printf("Pushed to stack: word='%s', candidates='%s', stack_top=%" PRIu64 "\n",
         word, candidates, stack_top);
  return 1;
}

void pop_from_stack(char *word, uint64_t *word_len, uint64_t *k, uint64_t *j,
                    uint64_t *l, char *candidates, uint64_t candidates_len,
                    uint64_t max_len) {
  if (!stack_len) {
    printf("Stack is empty, cannot pop.\n");
    return;
  }

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

  printf("Popped from stack: word='%s', candidates='%s'\n", word, candidates);
}

void generate_combination(const char *full_word, int full_word_length,
                          char *combination, char *candidate,
                          int candidate_length, char **word_list, uint64_t *idx,
                          int bottom, struct anagram_context *anagram_data) {
  
  if (has_match(word_list, (char *)full_word, full_word_length, idx, bottom)) {
    printf("Match found for %s at %" PRIu64 ", %s\n", full_word, *idx,
           word_list[*idx]);
    if (full_word_length > 2 && word_list[*idx][full_word_length] == 0) {
      append_word(&anagram_data->possible_anagrams,
                  anagram_data->num_anagrams_found,
                  &anagram_data->anagrams_capacity, word_list[*idx], 0);
      anagram_data->num_anagrams_found++;
      printf("Found anagram: %s\n", word_list[*idx]);
      word_list[*idx] = NULL;
      *idx += 1;
    }
  } else {
    return;
  }
  
  if (!candidate_length)
    return;
    
  char unused_candidates[candidate_length];

  for (int i = 0; i < candidate_length; i++) {
    if (i)
      memcpy(unused_candidates, candidate, i);
    memcpy(unused_candidates + i, candidate + i + 1, candidate_length - 1 - i);
    unused_candidates[candidate_length - 1] = 0;
    combination[0] = candidate[i];
    combination[1] = 0;
    printf("%s ... %s\n", full_word, unused_candidates);
    generate_combination(full_word, full_word_length + 1, combination + 1,
                         unused_candidates, candidate_length - 1, word_list,
                         idx, bottom, anagram_data);
  }
}

void *do_anagram(struct anagram_context *anagram_data) {

  // initial letter
  char word[anagram_data->input_word_length + 1];
  word[0] = anagram_data->unique_letter;
  word[1] = 0;

  printf("Initial letter set to: %c\n", anagram_data->unique_letter);

  // remove initial letter
  uint64_t candidate_length = anagram_data->input_word_length - 1;
  char new_anagram_candidates[candidate_length + 1];
  printf("Removing initial letter, candidate length: %" PRIu64 "\n",
         candidate_length);
  copy_non_interest_anagram_candidates(
      anagram_data->sorted_word, anagram_data->input_word_length,
      new_anagram_candidates,
      anagram_data->unique_letter_indices[anagram_data->thread_index]);
  new_anagram_candidates[candidate_length] = 0;

  printf("New anagram candidates: %s\n", new_anagram_candidates);

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

  printf("Loading words from dictionary...\n");

  while (fgets(buffer, sizeof(buffer), anagram_data->dictionary_file)) {
    printf("Checking word: %s", buffer);

    if (buffer[1] == new_anagram_candidates[i]) {
      if (!within_match) {
        within_match = 1;
        tops[i] = idx;
        printf("Match started at idx: %" PRIu64 "\n", idx);
      }
      len = strlen(buffer) - 1;
      if (len > anagram_data->input_word_length) {
        printf("Skipping longer word: %s", buffer);
        continue;
      }
      if (buffer[len] == '\n')
        buffer[len] = 0;
      if (append_word(&words, idx++, &limit, buffer, len)) {
        printf("Could not append new word\n");
        return NULL;
      }
    } else {
      if (within_match) {
        within_match = 0;
        bottoms[i] = idx - 1;
        printf("Match ended at idx: %" PRIu64 "\n", idx - 1);
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
  printf("Permutations starting with word: %s|%" PRIu64 "|%s\n", word,
         anagram_data->thread_index, new_anagram_candidates);

  uint64_t bottom = idx;
  idx = 0;

  char candidates[candidate_length + 1];
  memcpy(candidates, new_anagram_candidates, sizeof(candidates));
  candidates[candidate_length] = 0;

  // Debug loop start
  printf("Starting permutation loop...\n");

  printf("%s ...\n", candidates);

  generate_combination(word, 1, word + 1, candidates, candidate_length, words,
                       &idx, bottom, anagram_data);

  // Debug thread count update
  printf("Updating thread count...\n");
  update_thread_count(-1);

  return NULL;
}

void sort(char *word, uint64_t word_len) {
  printf("Entering sort function. Word: %s, Length: %" PRIu64 "\n", word,
         word_len);

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
        printf("Swapping %c and %c. Current state: %s\n", word[i], word[i - 1],
               word);
        i--;
      }
      if (!i)
        i = 1;
    }
  }

  printf("Exiting sort function. Sorted word: %s\n", word);
}

void unique(char *unik, int *idxs, uint64_t word_len) {
  printf("Entering unique function. Word length: %" PRIu64 "\n", word_len);

  uint64_t i = 1;
  uint64_t j = 1;
  while (i < word_len) {
    printf("Current unique index: %" PRIu64 ", Comparing: %c and %c\n", i,
           unik[i - 1], unik[i]);
    while (unik[i - 1] == unik[i]) {
      i++;
    }
    idxs[j] = i;
    unik[j++] = unik[i++];
  }
  unik[j] = 0;

  printf("Exiting unique function. Resulting unique string: %s\n", unik);
}

char **anagram(const char *word, uint64_t *len) {
  printf("Starting anagram generation for word: %s\n", word);

  uint64_t word_len = strlen(word);
  char upper[word_len + 1];
  memcpy(upper, word, word_len + 1);

  char *uc = upper;
  while (*uc) {
    *uc = tolower(*uc);
    uc++;
  }
  printf("Converted to lowercase: %s\n", upper);

  sort(upper, word_len);

  char unik[word_len + 1];
  memcpy(unik, upper, word_len + 1);
  int idxs[word_len + 1];
  idxs[0] = 0;

  unique(unik, idxs, word_len);
  uint64_t uniq_len = strlen(unik);

  printf("Unique characters: %s, Unique length: %" PRIu64 "\n", unik, uniq_len);

  struct anagram_context anagram_data[uniq_len];
  pthread_t anagram_threads[uniq_len];

  for (uint64_t i = 0; i < uniq_len; i++) {
    char fpath[256];
    snprintf(fpath, sizeof(fpath), "%s/words/%c_words.txt",
             words_directory_path ? words_directory_path
                                  : DEFAULT_WORDS_DIRECTORY,
             unik[i]);
    printf("Thread %" PRIu64 ": Opening dictionary file: %s\n", i, fpath);

    anagram_data[i].dictionary_file = fopen(fpath, "r");
    if (!anagram_data[i].dictionary_file) {
      printf("Failed to open dictionary file: %s\n", fpath);
      return NULL;
    }
    anagram_data[i].sorted_word = upper;
    anagram_data[i].unique_letter_indices = idxs;
    anagram_data[i].anagrams_capacity = 20;
    anagram_data[i].possible_anagrams =
        malloc(anagram_data[i].anagrams_capacity * sizeof(char *));
    anagram_data[i].num_anagrams_found = 0;
    anagram_data[i].input_word_length = word_len;
    anagram_data[i].thread_index = i;
    anagram_data[i].unique_letter = unik[i];

    update_thread_count(+1);
    if (pthread_create(anagram_threads + i, NULL, (void *)do_anagram,
                       anagram_data + i)) {
      printf("Failed to create thread %" PRIu64 "\n", i);
      update_thread_count(-1);
    } else {
      printf("Successfully created thread %" PRIu64 "\n", i);
    }
  }

  // while (1) {
  //   sleep(1);
  //   if (active_thread_count <= 0)
  //     break;
  // }

  for (uint64_t i = 0; i < uniq_len; i++) {
    pthread_join(anagram_threads[i], NULL);
    fclose(anagram_data[i].dictionary_file);
    printf("Thread %" PRIu64 " joined and dictionary file closed with %" PRIu64
           " matches.\n",
           i, anagram_data[i].num_anagrams_found);
  }

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
          printf("Failed to reallocate memory for anagrams.\n");
          return NULL;
        }
        anagram_data[0].possible_anagrams = tmp;
      }

      memcpy(anagram_data[0].possible_anagrams +
                 anagram_data[0].num_anagrams_found,
             anagram_data[i].possible_anagrams,
             anagram_data[i].num_anagrams_found * sizeof(char *));
      anagram_data[0].num_anagrams_found += anagram_data[i].num_anagrams_found;
    }
    free(anagram_data[i].possible_anagrams);
  }

  printf("Anagram generation complete. Total anagrams found: %" PRIu64 "\n",
         anagram_data[0].num_anagrams_found);

  *len = anagram_data[0].num_anagrams_found;
  return anagram_data[0].possible_anagrams;
}

char set_words_directory_path(const char *path) {
  printf("Setting words directory path: %s\n", path);

  if (!path) {
    printf("No path provided. Exiting with failure.\n");
    return 0;
  }
  if (words_directory_path) {
    printf("Freeing previous path: %s\n", words_directory_path);
    free(words_directory_path);
  }
  uint64_t pathlen = strlen(path);
  words_directory_path = malloc(pathlen + 1);
  memcpy(words_directory_path, path, pathlen + 1);

  printf("Words directory path set to: %s\n", words_directory_path);
  return 1;
}

__attribute__((destructor)) void release_word_path() {
  if (words_directory_path) {
    printf("Releasing words directory path: %s\n", words_directory_path);
    free(words_directory_path);
    words_directory_path = NULL;
  } else {
    printf("No words directory path to release.\n");
  }
}

#ifdef ZO
int main() {
  // FILE *english_words = fopen("english-words-master/");
  char *word = "sally";
  uint64_t len;

  char **a = anagram(word, &len);
  for (int i = 0; i < len; i++) {
    puts(a[i]);
  }

  printf("%" PRIu64 " \n", len);
}
#endif