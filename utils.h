
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifndef UTILS_H
#define UTILS_H
#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif
typedef struct {
  uint8_t *data;
  ssize_t len;
  ssize_t size;
} vector;
void release_vector(vector **vec);

vector *get_vector(size_t size, uint8_t *initial_data, size_t initial_data_len);
char *join(size_t n, char *sep, ...);
char *unescape(char *string);
char *home_dir();
#define path_join(...)                                                         \
  join(VA_ARGS_COUNT(__VA_ARGS__), PATH_SEPARATOR, __VA_ARGS__)

/**
 * skidded :3
 */
#define VA_ARGS_COUNT(...)                                                     \
  VA_ARGS_SHIFT_COUNT_VALUES(__VA_ARGS__, VA_ARGS_COUNT_VALUES)

#define VA_ARGS_COUNT_VALUES                                                   \
  63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,  \
      44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27,  \
      26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9,   \
      8, 7, 6, 5, 4, 3, 2, 1, 0

#define VA_ARGS_SELECT_COUNT_VALUE(                                            \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16,     \
    _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, \
    _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, \
    _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, \
    _62, _63, N, ...)                                                          \
  N

#define VA_ARGS_SHIFT_COUNT_VALUES(...) VA_ARGS_SELECT_COUNT_VALUE(__VA_ARGS__)

#endif
