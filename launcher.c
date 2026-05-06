#include "json.h"
#include "utils.h"
#include <curl/curl.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static bool VERBOSE = false;

#define lprint(...)                                                            \
  printf("\x1b[38;2;255;85;85mLilith \x1b[38;2;85;85;85m> "                    \
         "\x1b[38;2;170;170;170m" __VA_ARGS__ "\x1b[0m")

#define debug(...)                                                             \
  do {                                                                         \
    if (VERBOSE)                                                               \
      printf(__VA_ARGS__);                                                     \
  } while (0)
/**
 * will exit(1) on failure
 */
bool create_dir(char *path) {
  if (mkdir(path, 0777) == -1 && errno != EEXIST) {
    lprint("Could not create launcher folder at: '%s'%s\n", path, );
    return false;
  }
  return true;
}
typedef struct download {
  FILE *output;
  size_t total;
  size_t done;
  int last_percent;
} download;
size_t save_data(char *buf, size_t size, size_t nmemb, void *ptr) {
  download *dl = (download *)ptr;
  size_t n_written = fwrite(buf, size, nmemb, dl->output);
  dl->total += n_written * size;
  int percentage = (int)(100 * ((double)dl->total / (double)(dl->done)));
  if (percentage >= dl->last_percent) {
    lprint("Downloading...    ");
    printf("\x1b[2D\x1b[38;2;170;170;170m%3i%%\r", percentage);
    fflush(stdout);
    dl->last_percent = percentage;
  }
  return n_written * size;
}

// size_t size is always one
size_t write_data(char *buf, size_t size, size_t nmenb, vector *vec) {
  (void)size;
  if ((ssize_t)(nmenb + vec->len) >= vec->size) {
    vec->data = realloc(vec->data, (nmenb + vec->len) * 2);
    if (!vec->data)
      return 0;
    vec->size = (nmenb + vec->len) * 2;
  }
  memcpy(vec->data + vec->len, buf, nmenb);
  vec->len += nmenb;
  vec->data[vec->len] = 0;
  return nmenb;
}

int parse_status_code(char *headers) {
  while (*headers != ' ')
    headers++;
  return strtol(headers, NULL, 10);
}

bool run_executable(char *path, char **args) {

  pid_t pid = fork();
  if (pid < 0)
    return false;
  if (pid == 0) {
    execvp(path, args);
    exit(1);
  }
  int status;
  wait(&status);
  if (WIFEXITED(status)) {
    if (WEXITSTATUS(status) == 0)
      return true;
  }
  return false;
}
void show_cursor() { printf("\x1b[?25h"); }

bool includes_string(int c, char **strs, char *incl) {
  c--;
  while (c >= 0 && strcmp(incl, strs[c]) != 0)
    c--;
  return c >= 0;
}

char *read_file(char *path) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  rewind(file);

  char *buf = malloc(file_size + 1);
  fread(buf, 1, file_size, file);
  buf[file_size] = 0;
  fclose(file);
  return buf;
}

int main(int argc, char **argv) {
  VERBOSE |= includes_string(argc, argv, "-V");
  VERBOSE |= includes_string(argc, argv, "--verbose");

  char *tmp = NULL;

  CURL *curl = NULL;
  vector *headers = NULL;
  long release_status_code = 0;
  vector *release_response = NULL;

  JSONElement *release_json = NULL;

  JSONElement *release_url = NULL;
  JSONElement *release_size = NULL;
  JSONElement *release_version = NULL;
  JSONElement *release_changelog = NULL;

  char *lilith_dir = NULL;
  char *lilith_exec_path = NULL;
  FILE *lilith_exec_new = NULL;

  char *config_path = NULL;
  char *raw_config = NULL;
  JSONElement *config_json = NULL;
  JSONElement *config_alpha_enabled = NULL;
  bool alpha_enabled = false;

  char *home = home_dir();

  if (!home) {
    lprint("Could not get home directory. Is your platform supported?\n");
    goto error;
  }
  debug("home_dir: %s\n", home);

  lilith_dir = path_join(home, ".lilith");
  debug("lilith_dir: %s\n", lilith_dir);
  if (!create_dir(lilith_dir)) {
    lprint("Could not create necessary Lilith directory!\n");
    goto error;
  };

  config_path = path_join(lilith_dir, "config.json");
  debug("config_path: %s\n", config_path);
  if (access(config_path, F_OK) == 0) {
    raw_config = read_file(config_path);
    if (!raw_config) {
      debug("could not read config file despite existing!\n");
    } else {
      debug("raw_config: '%s'\n", raw_config);

      config_json = JSON_parse(raw_config);
      if (!config_json) {
        debug("failed to parse config\n");
      } else {
        config_alpha_enabled = JSON_get("alpha", config_json);
        if (!config_alpha_enabled) {
          debug("couldnt get alpha field\n");
        } else if (config_alpha_enabled->JSONType == JSONType_BOOLEAN) {
          alpha_enabled = config_alpha_enabled->value.boolean;
        }
      }
    }
  }

  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    lprint("Could not initialize libcurl!");
    goto error;
  }
  curl = curl_easy_init();
  if (!curl) {
    lprint("Failed to get cURL handle!");
    goto error;
  }
  if (alpha_enabled) {
    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://api.lilith.rip/versions/"
                     "alpha?compact_changelog=true&digests=true");
  } else {
    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://api.lilith.rip/versions/"
                     "latest?compact_changelog=true&digests=true");
  }

  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);

  release_response = get_vector(1500, NULL, 0);
  headers = get_vector(100, NULL, 0);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, release_response);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, headers);

  if (curl_easy_perform(curl) != CURLE_OK) {
    lprint("Failed to perform version request!");
    goto error;
  };

  debug("%s%s\n", headers->data, release_response->data);

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &release_status_code);
  debug("status_code: %li\n", release_status_code);
  if (release_status_code != 200) {
    lprint("Could not fetch latest releases!");
    goto error;
  }

  release_json = JSON_parse((char *)release_response->data);

  release_version = JSON_get("version", release_json);
  release_changelog = JSON_get("changelog", release_json);

#ifdef __APPLE__
  release_url = JSON_get(JSON_key("download.macos"), release_json);
  release_size = JSON_get(JSON_key("sizes.macos"), release_json);
#elif defined(_WIN32)
  release_url = JSON_get(JSON_key("download.windows"), release_json);
  release_size = JSON_get(JSON_key("sizes.windows"), release_json);
#else
  release_url = JSON_get(JSON_key("download.linux"), release_json);
  release_size = JSON_get(JSON_key("sizes.linux"), release_json);
#endif
  if (!release_url || !release_size) {
    lprint("Could not find a release for you OS. Is your platform "
           "supported?\n");
    goto error;
  }

  debug("release_url: '%s'\n", release_url->value.string);
  debug("release_changelog: '%s'\n", release_changelog->value.string);
  debug("release_version: '%s'\n", release_version->value.string);

  char *token = NULL;
  char *start = NULL;
  tmp = start = strdup(release_url->value.string);

  while ((token = strsep(&start, "/")) != NULL && start != NULL)
    ;
  if (!token) {
    lprint("Failed to parse filename.\n");
    goto error;
  }
  lilith_exec_path = path_join(lilith_dir, token);
  free(tmp);
  tmp = NULL;
  if (access(lilith_exec_path, F_OK) != 0) {
    tmp = strdup(release_changelog->value.string);
    char *unescaped_cl = unescape(tmp);

    lprint("New version found!\n");
    printf("===========================\n");
    printf("%s\n", unescaped_cl);
    printf("===========================\n");
    free(tmp);
    tmp = NULL;

    lilith_exec_new = fopen(lilith_exec_path, "w");
    if (!lilith_exec_new) {
      lprint("Could not access Lilith internals.\n");
      goto error;
    }

    download dl = {0};
    dl.output = lilith_exec_new;
    dl.done = (size_t)release_size->value.number;

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, release_url->value.string);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_data);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dl);
    headers->len = 0;

    if (curl_easy_perform(curl) != CURLE_OK) {
      lprint("Failed to download latest Lilith version!\n");
      goto error;
    };

    printf("\x1b[2k\r");
    lprint("Downloaded newest release: %s%s\n",
           release_version->value.string, );
    debug("download_headers: '%s'\n", headers->data);

    fclose(lilith_exec_new);
    lilith_exec_new = NULL;
  }

#ifdef __APPLE__
  char *quarantine_args[] = {"xattr", "-cr", lilith_exec_path, NULL};
  if (!run_executable("xattr", quarantine_args)) {
    lprint("Could not remove quarantine flags!\n");
    goto error;
  }
  debug("quarantine: success\n");
#endif
#ifndef _WIN32
  char *chmod_flags[] = {"chmod", "+x", lilith_exec_path, NULL};
  if (!run_executable("chmod", chmod_flags)) {
    lprint("Could not make lilith an executable!\n");
    goto error;
  }
  debug("chmod: success\n");
#endif
  int tries = 0;
  while (tries < 3) {
    debug("launching lilith");
    char *lilith_flags[] = {lilith_exec_path, "--iknowwhatimdoing", NULL};
    if (!run_executable(lilith_exec_path, lilith_flags)) {
      lprint("Could not launch Lilith :(\n");
      if (tries < 2)
        lprint("Trying to relaunch...\n");
      tries++;
    }
  }
  lprint("Deleting Lilith...\n");
  lprint("You might want to retry after that (Lilith will be reinstalled on "
         "relaunch).\n");
  if (remove(lilith_exec_path) != 0) {
    lprint("Couldn't remove Lilith, you might want to run the following in "
           "terminal:\n");
    lprint("`rm %s`\n%s", lilith_exec_path, );
  }

  goto error;
error:
  if (errno != 0)
    perror("Error");
  lprint("Exiting in 15 seconds.\n");
  sleep(15);
  goto cleanup;
cleanup:
  free(tmp);

  curl_easy_cleanup(curl);

  release_vector(&headers);
  release_vector(&release_response);

  JSON_free(release_json);

  free(lilith_dir);
  free(lilith_exec_path);
  fclose(lilith_exec_new);

  free(config_path);
  free(raw_config);
  JSON_free(config_json);

  exit(0);
}
