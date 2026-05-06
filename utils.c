#include "utils.h"
#include <pwd.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
/* mr penis */
signed char replacer[256] = {
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    '"',  0, 0,    0,    0,    '\'', 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    '?',
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, '\\', 0, 0,    0,
    0, '\a', '\b', 0, 0,    '\e', '\f', 0,    0, 0, 0, 0, 0,    0, '\n', 0,
    0, 0,    -1,   0, '\t', 0,    '\v', 0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
    0, 0,    0,    0, 0,    0,    0,    0,    0, 0, 0, 0, 0,    0, 0,    0,
};
void release_vector(vector **vec) {
  if ((*vec)) {
    free((*vec)->data);
    free((*vec));
    *vec = NULL;
  }
}

vector *get_vector(size_t size, uint8_t *initial_data,
                   size_t initial_data_len) {
  vector *vec = malloc(sizeof(vector));
  vec->data = malloc(size);
  if (!vec->data) {
    free(vec);
    return NULL;
  }
  vec->size = size;
  vec->len = 0;
  if (initial_data) {
    memcpy(vec->data, initial_data, initial_data_len);
  }
  return vec;
}
size_t append_vector(vector *vec, uint8_t *data, size_t nb) {
  if ((ssize_t)(nb + vec->len) >= vec->size) {
    vec->data = realloc(vec->data, (nb + vec->len) * 2);
    if (!vec->data)
      return 0;
    vec->size = (nb + vec->len) * 2;
  }
  memcpy(vec->data + vec->len, data, nb);
  vec->len += nb;
  vec->data[vec->len] = 0;
  return nb;
}
/**
 * Returns the string path of the current user’s home directory.
 * On POSIX, it uses the $HOME environment variable if defined. Otherwise it
 * uses the effective UID to look up the user’s home directory. On Windows, it
 * uses the USERPROFILE environment variable if defined. Otherwise it uses the
 * path to the profile directory of the current user.
 */
char *home_dir() {
#ifdef __WIN32
  printf("not supported yet!\n");
  exit(1);
#else
  char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    home = pw->pw_dir;
  }
  return home;
#endif
}

/**
 * only sep[0] is taken into account
 */
char *join(size_t n, char *sep, ...) {
  va_list args;
  va_start(args, sep);
  vector *vec = get_vector(64, NULL, 0);

  for (size_t i = 0; i < n; i++) {
    char *current_str = va_arg(args, char *);
    if (*current_str != *sep)
      append_vector(vec, (uint8_t *)sep, 1);
    append_vector(vec, (uint8_t *)current_str, strlen(current_str));
  }
  va_end(args);
  char *res = (char *)vec->data;
  free(vec);
  return res;
}

/**
 * unescapes a string in place
 * only unescapes basic chars like `\n`, `\t` etc.
 * `\r` is removed
 */
char *unescape(char *string) {
  size_t c = 0;
  size_t r = 0;
  while (string[c] != '\0') {
    if (string[c] == '\\') {
      if (replacer[(unsigned char)string[c + 1]] != 0) {
        if (replacer[(unsigned char)string[c + 1]] != -1)
          string[r++] = replacer[(unsigned char)string[c + 1]];
        c = c + 2;
      } else {
        string[r++] = string[c++];
      }
    } else {
      string[r++] = string[c++];
    }
  }
  string[r] = 0;
  return string;
}
