#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sort(char *word) {
  if (!*word)
    return;
  char *a = word;
  char *b = word + 1;
  while (*b && *a > *b) {
    b++;
  }
  if (--b >= (a + 1)) {
    char c = *a;
    memmove(a, a + 1, b - a);
    *b = c;
    sort(a);
    sort(b);
    sort(a);
  } else if (a[1] != 0) {
    sort(a + 1);
  }
}

void unique(char *word, int *idxs) {
  if (!*word)
    return;
  idxs[1] = idxs[0] + 1;
  char *a = word + 1;
  while (*word == *a) {
    a++;
    idxs[1]++;
  }
  if (a != (word + 1)) {
    memmove(word + 1, a, strlen(a) + 1);
  }
  ++idxs;
  return unique(word + 1, idxs);
}

struct anagram_st {
  FILE *words_file;
  char *xters;
  int *idxs;
  char **guesses;
  size_t guesses_len;
  size_t guesses_limit;
  size_t xter_len;
  size_t thread_idx;
};

char *copy_non_interest_anagram_candidates(const char *anagram_candidates,
                                           const size_t anagram_candidates_len,
                                           char *new_anagram_candidates,
                                           size_t i) {
  int j = 0;
  for (; j < i; j++) {
    new_anagram_candidates[j] = anagram_candidates[j];
  }
  // exclude candidate of interest
  for (int k = i + 1; k < anagram_candidates_len; k++) {
    new_anagram_candidates[j++] = anagram_candidates[k];
  }
  return new_anagram_candidates;
}

int append_word(char ***words, size_t idx, size_t *limit, char *anagram,
                size_t anagram_len) {
  if (*limit <= idx) {
    void *tmp = realloc(*words, sizeof(char *) * (*limit += *limit / 2));
    if (!tmp)
      return -1;
    *words = tmp;
  }

  (*words)[idx] = malloc((anagram_len + 1) * sizeof(char));
  memcpy((*words)[idx], anagram, anagram_len);

  return 0;
}

char has_match(char **words, char *anagram, size_t anagram_len, size_t *idx,
               size_t bottom) {

  int cmp;
  while ((cmp = strncmp(words[*idx], anagram, anagram_len))) {
    if (*idx > bottom || cmp > 0) {
      return 0;
    }
    *idx += 1;
  }
  return 1;
}

void actual_anagram(char **words, size_t *tops, size_t *bottoms, char *root,
                    size_t root_len, char ***guesses, size_t *guesses_limit,
                    int root_idx, char *anagram_candidates,
                    size_t anagram_candidates_len) {

  int anagram_len = root_len + 1;
  for (int i = 0; i < anagram_candidates_len; i++) {

    if (root_len == 1 && bottoms[i] == 0)
      continue;

    // append letter of interest
    root[root_len] = anagram_candidates[i];
    // copy candidates
    char new_anagram_candidates[anagram_candidates_len - 1];
    copy_non_interest_anagram_candidates(
        anagram_candidates, anagram_candidates_len, new_anagram_candidates, i);
    // check word
    size_t *_top = &tops[root_len == 1 ? i : 0];
    size_t _bottom = bottoms[root_len == 1 ? i : 0];
    if (has_match(words, root, anagram_len, _top, _bottom)) {
      char word[anagram_len + 1];
      memcpy(word, root, anagram_len);
      word[anagram_len] = 0;
      if (anagram_len > 2 && words[*_top][anagram_len] == 0) {
        append_word(guesses, root_idx++, guesses_limit, root, anagram_len);
      }
      actual_anagram(words, _top, &_bottom, word, anagram_len, guesses,
                     guesses_limit, root_idx, new_anagram_candidates,
                     anagram_candidates_len - 1);
    }
  }
  root[root_len] = 0;
}

void *do_anagram(struct anagram_st *anagram_data) {
  // initial letter
  char word[2] = {
      anagram_data->xters[anagram_data->idxs[anagram_data->thread_idx]], 0};
  // remove initial letter
  char new_anagram_candidates[anagram_data->xter_len];
  copy_non_interest_anagram_candidates(
      anagram_data->xters, anagram_data->xter_len, new_anagram_candidates,
      anagram_data->idxs[anagram_data->thread_idx]);

  size_t tops[anagram_data->xter_len];
  size_t bottoms[anagram_data->xter_len];

  memset(tops, 0, sizeof(tops));
  memset(bottoms, 0, sizeof(bottoms));

  char buffer[1024];
  int i = 0;
  size_t limit = 100;
  size_t idx = 0;
  size_t len = 0;
  char within_match = 0;
  char **words = malloc(limit * sizeof(*words));
  while (fgets(buffer, sizeof(buffer), anagram_data->words_file)) {
    if (buffer[1] == new_anagram_candidates[i]) {
      if (!within_match) {
        within_match = 1;
        tops[i] = idx;
      }
      len = strlen(buffer) - 1;
      buffer[len] = 0;
      if (append_word(&words, idx++, &limit, buffer, len)) {
        puts("Could not append new word");
        return NULL;
      }
    } else {
      if (within_match) {
        within_match = 0;
        bottoms[i] = idx - 1;
        while (new_anagram_candidates[i] == new_anagram_candidates[i + 1]) {
          // bottoms[i + 1] = bottoms[i];
          // tops[i + 1] = tops[i];
          i++;
        }
        i++;

        if (i >= (anagram_data->xter_len - 1))
          break;
      }
    }
  }
  // append
  *anagram_data->guesses = word;

  actual_anagram(words, tops, bottoms, word, 1, &anagram_data->guesses,
                 &anagram_data->guesses_limit, 0, new_anagram_candidates,
                 anagram_data->xter_len - 1);
  return NULL;
}

void anagram(const char *word) {
  size_t word_len = strlen(word);
  char upper[word_len + 1]; // = malloc(word_len + 1);
  memcpy(upper, word, word_len + 1);
  char *uc = upper;
  while (*uc) {
    *uc = tolower(*uc);
    uc++;
  }
  sort(upper);
  char unik[word_len + 1]; //= malloc(word_len + 1);
  memcpy(unik, upper, word_len + 1);
  int idxs[word_len + 1];
  idxs[0] = 0;
  unique(unik, idxs);
  size_t uniq_len = strlen(unik);

  for (int i = 0; i < uniq_len; i++) {
    char fpath[256];
    snprintf(fpath, sizeof(fpath), "words/%c_words.txt", unik[i]);

    struct anagram_st anagram_data = {
        fopen(fpath, "r"), upper, idxs, malloc(100 * sizeof(char *)), 0, 100,
        word_len,          i};
    pthread_t anagram_thread;
    int retval = pthread_create(&anagram_thread, NULL, (void *)do_anagram,
                                &anagram_data);
    pthread_join(anagram_thread, NULL);
  }
}

int main() {
  // FILE *english_words = fopen("english-words-master/");
  char *word = "sally";
  anagram(word);
}