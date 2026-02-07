/*
 * drive_ls.c
 *
 * List files in a Google Drive folder (parent ID), similar to "ls".
 * Uses Google Drive API v3 and follows nextPageToken for large folders.
 *
 * Requirements:
 * - access token stored in TOKEN_FILE (single line), kept up-to-date externally
 * - libcurl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define TOKEN_FILE "/home/www/data/google_access_token"

struct mem {
  char *ptr;
  size_t len;
};

static void mem_init(struct mem *m) {
  m->len = 0;
  m->ptr = (char*)malloc(1);
  if (m->ptr) m->ptr[0] = '\0';
}

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct mem *m = (struct mem*)userp;

  char *p = (char*)realloc(m->ptr, m->len + realsize + 1);
  if (!p) return 0;

  m->ptr = p;
  memcpy(&(m->ptr[m->len]), contents, realsize);
  m->len += realsize;
  m->ptr[m->len] = '\0';
  return realsize;
}

static int read_access_token(char *buf, size_t buflen) {
  FILE *fp = fopen(TOKEN_FILE, "r");
  if (!fp) {
    fprintf(stderr, "Error: cannot open token file %s\n", TOKEN_FILE);
    return 0;
  }

  if (!fgets(buf, (int)buflen, fp)) {
    fprintf(stderr, "Error: cannot read token from %s\n", TOKEN_FILE);
    fclose(fp);
    return 0;
  }
  fclose(fp);

  /* strip trailing newline(s) */
  size_t n = strlen(buf);
  while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) {
    buf[n-1] = '\0';
    n--;
  }

  if (buf[0] == '\0') {
    fprintf(stderr, "Error: empty token in %s\n", TOKEN_FILE);
    return 0;
  }
  return 1;
}

/* Extract nextPageToken from JSON response into out (or set out to empty). */
static int extract_next_page_token(const char *json, char *out, size_t outsz) {
  if (!json || !out || outsz == 0) return 0;
  out[0] = '\0';

  const char *p = strstr(json, "\"nextPageToken\"");
  if (!p) return 1; /* not found = no more pages, but not an error */

  p = strchr(p, ':');
  if (!p) return 0;
  p++;

  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
  if (*p != '"') return 0;
  p++;

  const char *e = strchr(p, '"');
  if (!e) return 0;

  size_t n = (size_t)(e - p);
  if (n + 1 > outsz) return 0;

  memcpy(out, p, n);
  out[n] = '\0';
  return 1;
}

/*
 * Very small/naive "parser" to print file names.
 * It prints every occurrence of: "name": "<...>"
 * This matches the minimalist approach used in the other tools.
 */
static void print_names(const char *json) {
  const char *p = json;

  while ((p = strstr(p, "\"name\"")) != NULL) {
    p = strchr(p, ':');
    if (!p) break;
    p++;

    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p != '"') continue;
    p++;

    const char *e = strchr(p, '"');
    if (!e) break;

    fwrite(p, 1, (size_t)(e - p), stdout);
    fputc('\n', stdout);

    p = e + 1;
  }
}

static int drive_list_folder(const char *parent_id) {
  CURL *curl = NULL;
  struct curl_slist *headers = NULL;
  char token[2048];
  char auth[4096];
  long http = 0;

  /* pagination */
  char page_token[2048];
  page_token[0] = '\0';

  if (!read_access_token(token, sizeof(token))) {
    return 1;
  }

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Error: curl_easy_init failed\n");
    return 1;
  }

  snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
  headers = curl_slist_append(headers, auth);

  for (;;) {
    char url[8192];
    struct mem body;

    /* Query: '<parent>' in parents and trashed=false */
    if (page_token[0]) {
      snprintf(url, sizeof(url),
        "https://www.googleapis.com/drive/v3/files"
        "?q='%s'+in+parents+and+trashed=false"
        "&fields=nextPageToken,files(name)"
        "&pageSize=1000"
        "&pageToken=%s"
        "&supportsAllDrives=true"
        "&includeItemsFromAllDrives=true",
        parent_id, page_token
      );
    } else {
      snprintf(url, sizeof(url),
        "https://www.googleapis.com/drive/v3/files"
        "?q='%s'+in+parents+and+trashed=false"
        "&fields=nextPageToken,files(name)"
        "&pageSize=1000"
        "&supportsAllDrives=true"
        "&includeItemsFromAllDrives=true",
        parent_id
      );
    }

    mem_init(&body);

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&body);

    /* SSL checks ON (same spirit as your tools) */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    /* reasonable timeouts */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "drive-ls/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "Error: HTTP request failed: %s\n", curl_easy_strerror(res));
      free(body.ptr);
      break;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);

    if (http < 200 || http >= 300) {
      fprintf(stderr, "Error: Drive API returned HTTP %ld\n", http);
      if (body.ptr && body.len > 0) {
        fprintf(stderr, "Body:\n%s\n", body.ptr);
      }
      free(body.ptr);
      break;
    }

    /* Print file names for this page */
    if (body.ptr && body.len > 0) {
      print_names(body.ptr);

      /* Get next page token */
      if (!extract_next_page_token(body.ptr, page_token, sizeof(page_token))) {
        fprintf(stderr, "Error: cannot parse nextPageToken\n");
        free(body.ptr);
        break;
      }
    } else {
      page_token[0] = '\0';
    }

    free(body.ptr);

    if (page_token[0] == '\0') {
      /* no more pages */
      break;
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <parent_folder_id>\n", argv[0]);
    fprintf(stderr, "Note: access token is read from %s\n", TOKEN_FILE);
    return 1;
  }

  curl_global_init(CURL_GLOBAL_DEFAULT);
  int rc = drive_list_folder(argv[1]);
  curl_global_cleanup();
  return rc;
}

