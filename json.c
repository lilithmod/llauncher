#include "json.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADVANCE_CURSOR(data, cursor)                                           \
  do {                                                                         \
    while (isspace(data[cursor]))                                              \
      cursor++;                                                                \
  } while (0)

#define PRINT_EL(key, val, pref)                                               \
  do {                                                                         \
    printf("%s%s%s%s\n", pref, key ? key : "", key ? ": " : "", val);          \
  } while (0)
#if JSON_DEBUG
#define debug(...)                                                             \
  do {                                                                         \
    printf(__VA_ARGS__);                                                       \
  } while (0)
#else
#define debug(...)
#endif

JSONElement *_JSON_parse(char *data, size_t *cursor_ptr) {
  debug("cursor: "
        "%zu"
        "\ndata: %s\n",
        cursor_ptr == NULL ? 0x0 : *cursor_ptr, data);
  debug("%s\n", data + (*cursor_ptr));

  ADVANCE_CURSOR(data, (*cursor_ptr));
  if (data[(*cursor_ptr)] == '\0') {
    printf("format error at cursor pos: %zu\n", (*cursor_ptr));
    return NULL;
  }
  JSONElement *element = malloc(sizeof(JSONElement));
  element->key = NULL;
  element->child_count = 0;
  if (data[(*cursor_ptr)] == '{') {
    (*cursor_ptr)++;
    element->JSONType = JSONType_OBJECT;
    while (data[(*cursor_ptr)] != '}') {
      ADVANCE_CURSOR(data, (*cursor_ptr));
      if (data[(*cursor_ptr)] != '"') {
        printf("58 | format error at cursor pos: %zu, expected `\"`\n",
               (*cursor_ptr));
        free(element);
        return NULL;
      }

      char *key;
      size_t key_start = ++(*cursor_ptr);

      while (data[(*cursor_ptr)] != '"') {
        if (data[(*cursor_ptr)] == '\\')
          (*cursor_ptr)++;
        (*cursor_ptr)++;
      }
      if ((*cursor_ptr) - key_start == 0) {
        key = strdup("");
      } else {
        key = strndup(data + key_start, (*cursor_ptr) - key_start);
      }

      (*cursor_ptr)++;
      ADVANCE_CURSOR(data, (*cursor_ptr));
      if (data[(*cursor_ptr)] != ':') {
        printf("format error at cursor pos: %zu, expected `:`\n",
               (*cursor_ptr));
        free(element);
        return NULL;
      }
      (*cursor_ptr)++;
      ADVANCE_CURSOR(data, (*cursor_ptr));

      JSONElement *child = _JSON_parse(data, cursor_ptr);
      debug("exiting children(s) on char: `%c`\n", data[(*cursor_ptr)]);
      if (!child) {
        // the child errored
        return NULL;
      }

      child->key = key;

      if (element->child_count == 0) {
        element->value.object = malloc(sizeof(JSONElement));
      } else {
        element->value.object =
            realloc(element->value.object,
                    (element->child_count + 1) * sizeof(JSONElement));
      }

      if (!element->value.object) {
        perror("out of memory");
        free(element);
        return NULL;
      }

      element->value.object[element->child_count] = child;
      element->child_count = element->child_count + 1;
      ADVANCE_CURSOR(data, (*cursor_ptr));

      if (data[(*cursor_ptr)] == ',') {
        (*cursor_ptr)++;
        ADVANCE_CURSOR(data, (*cursor_ptr));
        if (data[(*cursor_ptr)] == '}') {
          printf("format error at cursor pos: %zu, expected JSONElement\n",
                 (*cursor_ptr));
          free(element);
          return NULL;
        }
      } else {
        if (data[(*cursor_ptr)] == '}') {
          break;
        } else {
          printf("format error at cursor pos: %zu, expected `,` OR `}`, but "
                 "found `%c`\n",
                 (*cursor_ptr), data[(*cursor_ptr)]);
          free(element);
          return NULL;
        }
      }
    }
    (*cursor_ptr)++;

    return element;
  }

  if (data[(*cursor_ptr)] == '[') {
    (*cursor_ptr)++;
    element->JSONType = JSONType_ARRAY;
    element->key = NULL;
    while (data[(*cursor_ptr)] != ']') {
      ADVANCE_CURSOR(data, (*cursor_ptr));

      JSONElement *child = _JSON_parse(data, cursor_ptr);
      debug("exiting children(s) on char: `%c`\n", data[(*cursor_ptr)]);
      if (!child) {
        // the child errored
        return NULL;
      }

      if (element->child_count == 0) {
        element->value.array = malloc(sizeof(JSONElement));
      } else {
        element->value.array =
            realloc(element->value.array,
                    (element->child_count + 1) * sizeof(JSONElement));
      }
      if (!element->value.array) {
        perror("out of memory");
        free(element);
        return NULL;
      }

      element->value.array[element->child_count] = child;
      element->child_count = element->child_count + 1;
      ADVANCE_CURSOR(data, (*cursor_ptr));

      if (data[(*cursor_ptr)] == ',') {
        (*cursor_ptr)++;
        ADVANCE_CURSOR(data, (*cursor_ptr));
        if (data[(*cursor_ptr)] == ']') {
          printf("format error at cursor pos: %zu, expected JSONElement\n",
                 (*cursor_ptr));
          free(element);
          return NULL;
        }
      } else {
        if (data[(*cursor_ptr)] == ']') {
          break;
        } else {
          printf("format error at cursor pos: %zu, expected `,` OR `]`, but "
                 "found `%c`\n",
                 (*cursor_ptr), data[(*cursor_ptr)]);
          free(element);
          return NULL;
        }
      }
    }
    (*cursor_ptr)++;
    return element;
  }

  if (data[(*cursor_ptr)] == '"') {
    element->JSONType = JSONType_STRING;
    size_t val_start = ++(*cursor_ptr);

    while (data[(*cursor_ptr)] != '"') {
      if (data[(*cursor_ptr)] == '\\')
        (*cursor_ptr)++;
      (*cursor_ptr)++;
    }
    if ((*cursor_ptr) - val_start == 0) {
      element->value.string = strdup("");
    } else {
      element->value.string =
          strndup(data + val_start, (*cursor_ptr) - val_start);
    }
    (*cursor_ptr)++;
    return element;
  }

  if (isdigit(data[(*cursor_ptr)])) {
    char *endptr;
    double val = strtod(data + (*cursor_ptr), &endptr);
    if (endptr == data + (*cursor_ptr)) {
      printf("format error at cursor pos: %zu, expected a number (double)\n",
             (*cursor_ptr));
      free(element);
      return NULL;
    }
    (*cursor_ptr) = endptr - data;
    element->JSONType = JSONType_NUMBER;
    element->value.number = val;
    return element;
  }

  if (strncmp("true", data + (*cursor_ptr), 4) == 0) {
    element->JSONType = JSONType_BOOLEAN;
    element->value.boolean = true;
    (*cursor_ptr) += 4;
    return element;
  }
  if (strncmp("false", data + (*cursor_ptr), 5) == 0) {
    element->JSONType = JSONType_BOOLEAN;
    element->value.boolean = false;
    (*cursor_ptr) += 5;
    return element;
  }
  if (strncmp("null", data + (*cursor_ptr), 4) == 0) {
    printf("null\n");
    element->JSONType = JSONType_NULL;
    element->value.boolean = false;
    (*cursor_ptr) += 4;
    return element;
  }

  printf("NO_MATCH | format error at cursor pos: %zu, expected a "
         "JSElement\ngot char: `%c`\n",
         (*cursor_ptr), data[(*cursor_ptr)]);
  free(element);
  return NULL;
}
JSONElement *JSON_parse(char *data) {
  size_t cursor = 0;
  return _JSON_parse(data, &cursor);
}

JSONElement *get_element_from_object(char *key, JSONElement *object) {
  if (object->JSONType != JSONType_OBJECT)
    return NULL;
  size_t i = 0;
  while (i < object->child_count &&
         strcmp(object->value.object[i]->key, key) != 0) {
    i++;
  }
  if (i == object->child_count)
    return NULL;
  return object->value.object[i];
}
JSONElement *JSON_get(char *key, JSONElement *element) {
  if (element == NULL)
    return NULL;
  if (!key || *key == '\0')
    return element;
  char *end = key;
  key = strsep(&end, ".");
  char *walker = key;
  while (*walker != '\0' && *walker != '[')
    walker++;
  if (*walker == '[') {
    char *array_index_start = walker;
    while (*walker != ']' && *walker != '\0')
      walker++;
    if (*walker == ']') {
      char *endptr;
      size_t index = strtoul(walker, &endptr, 10);
      if (endptr == walker) {
        if (element->JSONType == JSONType_OBJECT) {
          size_t i = 0;
          while (i < element->child_count &&
                 strncmp(element->value.object[i]->key, key,
                         key - array_index_start) != 0) {
            i++;
          }
          if (i == element->child_count) {
            return JSON_get(end, get_element_from_object(key, element));
          } else {
            if (element->value.object[i]->JSONType != JSONType_ARRAY) {
              return JSON_get(end, get_element_from_object(key, element));

            } else {
              if (index >= element->value.object[i]->child_count) {
                return JSON_get(end, get_element_from_object(key, element));
              } else {
                size_t inner_index = 0;
                while (inner_index < index) {
                  inner_index++;
                }
                return JSON_get(end, element->value.object[i]->value.array[i]);
              }
            }
          }
        }
      }
    }
  }
  return JSON_get(end, get_element_from_object(key, element));
}

void print_simple(char *key, char *val, char *pref) {
  printf("%s%s%s%s\n", pref, key ? key : "", key ? ": " : "", val);
}

void print_num(char *key, double val, char *pref) {
  printf("%s%s%s%f\n", pref, key ? key : "", key ? ": " : "", val);
}

void JSON_print(JSONElement *element, char *pref) {
  if (!element)
    return;
  switch (element->JSONType) {
  case JSONType_NULL: {
    PRINT_EL(element->key, "null", pref);
    break;
  }
  case JSONType_BOOLEAN: {
    if (element->value.boolean == true)
      PRINT_EL(element->key, "true", pref);
    else
      PRINT_EL(element->key, "false", pref);
    break;
  }
  case JSONType_NUMBER: {
    print_num(element->key, element->value.number, pref);
    break;
  }
  case JSONType_ARRAY:
  case JSONType_OBJECT: {

    if (element->key)
      print_simple(element->key, "", pref);
    for (size_t i = 0; i < element->child_count; i++)
      JSON_print(
          element->value.array[i],
          pref - 1); // same underlying type so who cares and for the pref - 1
    // pref is a big string where we just go back, really bad
    // for deeply nested objects but who cares
    break;
  }
  case JSONType_STRING: {
    print_simple(element->key, element->value.string, pref);
    break;
  }
  }
}

void JSON_free(JSONElement *element) {
  if (element->JSONType == JSONType_ARRAY ||
      element->JSONType == JSONType_OBJECT) {
    size_t i = 0;
    while (i < element->child_count) {
      JSON_free(element->value.array[i++]);
    }
    free(element->value.array);
  }
  if (element->JSONType == JSONType_STRING) {
    free(element->value.string);
  }

  free(element->key);
  free(element);
}

int tests() {
  // char *bb = malloc(1024);
  // bb = NULL;
  char *data =
      "{\"version\":\"v2.0.13\",\"name\":\"v2.0.13\",\"changelog\":\"# "
      "v2.0.13\\r\\n- Fix chatstats failing on untagged players,\\r\\n- Use "
      "default `/sc` gamemode on player "
      "statcheck\",\"download\":{\"windows\":\"https://github.com/lilithmod/"
      "releases/releases/download/v2.0.13/"
      "lilith-windows-2.0.13.exe\",\"macos\":\"https://github.com/lilithmod/"
      "releases/releases/download/v2.0.13/"
      "lilith-macos-2.0.13\",\"linux\":\"https://github.com/lilithmod/releases/"
      "releases/download/v2.0.13/"
      "lilith-linux-2.0.13\"},\"sizes\":{\"windows\":149713920,\"macos\":"
      "196997026,\"linux\":135215424},\"digests\":{\"windows\":\"sha256:"
      "4e02040f815cfa167dfd55adeebc3f2e19af6c2a89e8996737ff78ff8dc34f9e\","
      "\"macos\":\"sha256:"
      "764a194cf2ba70b89c53ded16e00d758de52939ba32ab5d7ea8f00852786a843\","
      "\"linux\":\"sha256:"
      "5e029b50f827340884a1c8575fcfc34a464c8ad1c960e8a4a88374bb08a0645f\"}}";
  size_t cu = 0;
  JSONElement *el = JSON_parse(data);
  char *pref = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
  printf("\n");
  JSON_print(el, pref + 28);
  JSONElement *version = JSON_get(JSON_key("version"), el);
  if (version) {
    if (version->JSONType == JSONType_STRING) {
      printf("'version' : '%s'\n", version->value.string);
    } else
      printf("unexpected type!\n");
  }
  JSONElement *win_download = JSON_get(JSON_key("download.windows"), el);
  if (win_download) {
    if (win_download->JSONType == JSONType_STRING) {
      printf("'download.windows' : '%s'\n", win_download->value.string);
    } else
      printf("unexpected type!\n");
  }
  JSON_free(el);
  return 0;
}

// int main(void) { return tests(); }
