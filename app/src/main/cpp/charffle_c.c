#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_TAG "charffle_c.so"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

char **anagram(const char *word, uint64_t *len);
char set_words_directory_path(const char *path);

jobjectArray shuffle(JNIEnv *env, __attribute__((unused)) jobject clazz,
                     jobject chars) {

  
  const char *xters = (*env)->GetStringUTFChars(env, chars, NULL);

  uint64_t n_matches = 0;
  char **matches = anagram(xters, &n_matches);
  (*env)->ReleaseStringUTFChars(env, chars, xters);

  jobjectArray jmatches = (jobjectArray)(*env)->NewObjectArray(
      env, (matches && n_matches) ? n_matches : 1,
      (*env)->FindClass(env, "java/lang/String"),
      (*env)->NewStringUTF(env, ""));

  if (matches) {
    for (uint64_t i = 0; i < n_matches; i++) {
      (*env)->SetObjectArrayElement(
          env, jmatches, i,
          (*env)->NewStringUTF(env, matches[i] ? matches[i] : "???"));
    }
  } else {
    (*env)->SetObjectArrayElement(
        env, jmatches, 0,
        (*env)->NewStringUTF(
            env, n_matches ? "A problem occured while computing anagrams"
                           : "No anagrams found"));
  }


  return jmatches;
}

jboolean register_private_path(JNIEnv *env,
                               __attribute__((unused)) jobject thiz,
                               jobject fdir, jobjectArray errmsg) {
  // Get the directory path from the Java string
  const char *dir = (*env)->GetStringUTFChars(env, fdir, NULL);
  if (!dir) {
    (*env)->SetObjectArrayElement(
        env, errmsg, 0,
        (*env)->NewStringUTF(env, "Failed to get directory path."));
    return JNI_FALSE;
  }

  // Register directory path to shuffle
  set_words_directory_path(dir);

  (*env)->ReleaseStringUTFChars(env, fdir, dir);
  return JNI_TRUE;
}

// Thread-local variable for the base name of the asset file
_Thread_local static const char *asset_base_name = "words/a_words.txt";

// Structure to hold extraction parameters
struct extract {
  AAssetManager *aam; // Android asset manager
  char *dir;          // Target directory for extracted files
  char *xters;        // Letters to extract (e.g., 'a', 'b', ...)
  char errmsg[256];   // Buffer to store error messages
  size_t xter_len;    // Number of letters to process
  char status;        // Status flag (success/failure)
};

// Worker function to extract assets based on letters in `xters`
void *extract_asset_worker(struct extract *e) {
  char buffer[4096]; // Buffer for reading asset data
  char asset[18];    // Buffer for asset file name (e.g., "words/a_words.txt")
  memcpy(asset, asset_base_name, 18); // Initialize asset name with base

  e->status = JNI_TRUE; // Default status is success

  // Loop through all letters in `xters`
  while ((e->xter_len--) && (asset[6] = *e->xters)) {
    // Open the asset for the current letter
    AAsset *asset_object =
        AAssetManager_open(e->aam, asset, AASSET_MODE_STREAMING);

    if (asset_object) {
      // Build the path for the internal file
      snprintf(buffer, sizeof(buffer), "%s/%s", e->dir, asset);

      // Open the target file for writing
      FILE *f = fopen(buffer, "wb");
      if (f == NULL) {
        snprintf(e->errmsg, sizeof(e->errmsg), "Failed to open file %s: %s",
                 buffer, strerror(errno));
        e->status = JNI_FALSE;
        break;
      }

      // Read asset data and write to the internal file
      int len = 0;
      while ((len = AAsset_read(asset_object, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, len, f);
      }

      fclose(f);                  // Close the target file
      AAsset_close(asset_object); // Close the asset
    } else {
      // Handle asset open failure
      snprintf(e->errmsg, sizeof(e->errmsg), "Failed to open asset %s: %s",
               asset, strerror(errno));
      e->status = JNI_FALSE;
      break;
    }

    e->xters++; // Move to the next letter
  }

  return NULL; // Worker completed
}

jboolean extract_words_asset(JNIEnv *env, __attribute__((unused)) jobject thiz,
                             jobject asset_man, jobject fdir,
                             jobjectArray errmsg) {
  // Get the directory path from the Java string
  const char *dir = (*env)->GetStringUTFChars(env, fdir, NULL);
  if (!dir) {
    if (errmsg)
      (*env)->SetObjectArrayElement(
          env, errmsg, 0,
          (*env)->NewStringUTF(env, "Failed to get directory path."));
    return JNI_FALSE;
  }

  // Register directory path to shuffle
  set_words_directory_path(dir);
  // register_private_path(env, thiz, fdir, errmsg);

  // Create the "words" directory inside the provided directory
  char word_dir[256];
  snprintf(word_dir, sizeof(word_dir), "%s/words/", dir);
  if (mkdir(word_dir, S_IRWXU | S_IRWXG | S_IRWXO) && errno != EEXIST) {
    if (errmsg)
      (*env)->SetObjectArrayElement(env, errmsg, 0,
                                    (*env)->NewStringUTF(env, strerror(errno)));
    (*env)->ReleaseStringUTFChars(env, fdir, dir);
    return JNI_FALSE;
  }

  // Get the Android asset manager from the Java object
  AAssetManager *aam = AAssetManager_fromJava(env, asset_man);
  if (aam == NULL) {
    if (errmsg)
      (*env)->SetObjectArrayElement(
          env, errmsg, 0,
          (*env)->NewStringUTF(env, "Failed to get AssetManager."));
    (*env)->ReleaseStringUTFChars(env, fdir, dir);
    return JNI_FALSE;
  }

  // Prepare threads and parameters
  char letters[] = "abcdefghijklmnopqrstuvwxyz";
  // todo: get n_threads from android or not
  int n_threads = 4;
  pthread_t pthreads[n_threads];
  struct extract e[n_threads];

  // Initialize and start threads
  for (int i = 0; i < n_threads; i++) {
    e[i].aam = aam;
    e[i].dir = (char *)dir;
    e[i].xters = letters + i * 8; // Divide letters among threads
    e[i].xter_len = 8;            // Each thread handles 8 letters
    e[i].status = JNI_TRUE;       // Default to success
    memset(e[i].errmsg, 0, sizeof(e[i].errmsg));

    // Create a thread to process the assets
    if (pthread_create(&pthreads[i], NULL, (void *)extract_asset_worker,
                       &e[i]) != 0) {
      if (errmsg)
        (*env)->SetObjectArrayElement(
            env, errmsg, 0,
            (*env)->NewStringUTF(env, "Failed to create thread."));
      (*env)->ReleaseStringUTFChars(env, fdir, dir);
      return JNI_FALSE;
    }
  }

  // Wait for all threads to finish
  for (int i = 0; i < n_threads; i++) {
    pthread_join(pthreads[i], NULL);
  }

  // Release the directory path
  (*env)->ReleaseStringUTFChars(env, fdir, dir);

  // Aggregate thread statuses and collect error messages
  for (int i = 0; i < n_threads; i++) {
    if (e[i].status == JNI_FALSE) {
      if (errmsg)
        (*env)->SetObjectArrayElement(env, errmsg, 0,
                                      (*env)->NewStringUTF(env, e[i].errmsg));
      return JNI_FALSE;
    }
  }

  // Return success if all threads succeeded
  return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, __attribute__((unused)) void *reserved) {
  JNIEnv *env = NULL;
  if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    LOGE("env unavailable; ");
    return JNI_ERR;
  }
  jclass clazz =
      (*env)->FindClass(env, "com/zeroone/charffle/CharffleActivity");
  if (!clazz) {
    LOGE("class unavailable; ");
    return JNI_ERR;
  }

  static const JNINativeMethod methods[] = {
      {"registerPrivatePath", "(Ljava/lang/String;[Ljava/lang/String;)Z",
       register_private_path},
      {"shuffle", "(Ljava/lang/String;)[Ljava/lang/String;", shuffle},
      {"extractWordsAsset",
       "(Landroid/content/res/AssetManager;Ljava/lang/String;[Ljava/lang/"
       "String;)Z",
       extract_words_asset}};
  int ret_code = (*env)->RegisterNatives(env, clazz, methods,
                                         sizeof(methods) / sizeof(*methods));
  if (ret_code != JNI_OK) {
    LOGE("could not register natives; ");
    return JNI_ERR;
  }
  LOGI("registered natives");

  return JNI_VERSION_1_6;
}