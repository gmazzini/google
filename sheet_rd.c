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
    fprintf(stderr, "Errore: impossibile aprire %s\n", TOKEN_FILE);
    return 0;
  }
  if (!fgets(buf, (int)buflen, fp)) {
    fclose(fp);
    fprintf(stderr, "Errore: impossibile leggere access token\n");
    return 0;
  }
  fclose(fp);
  buf[strcspn(buf, "\r\n")] = '\0';
  if (buf[0] == '\0') {
    fprintf(stderr, "Errore: access token vuoto\n");
    return 0;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  char access_token[512];
  char auth_header[1024];
  char url[2048];

  CURL *curl = NULL;
  CURLcode res;
  struct curl_slist *headers = NULL;

  struct mem body;
  long http = 0;

  if (argc != 3) {
    fprintf(stderr, "Uso: %s SPREADSHEET_ID RANGE\n", argv[0]);
    fprintf(stderr, "Esempio RANGE: Sheet1!A1:D10 oppure 'Foglio 1'!A:B\n");
    return 1;
  }

  if (!read_access_token(access_token, sizeof(access_token))) {
    return 1;
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    fprintf(stderr, "Errore: curl_global_init fallita\n");
    return 1;
  }

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Errore: curl_easy_init fallita\n");
    curl_global_cleanup();
    return 1;
  }

  // Importante: RANGE va URL-encoded
  char *enc_range = curl_easy_escape(curl, argv[2], 0);
  if (!enc_range) {
    fprintf(stderr, "Errore: curl_easy_escape(range) fallita\n");
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 1;
  }

  snprintf(url, sizeof(url),
           "https://sheets.googleapis.com/v4/spreadsheets/%s/values/%s",
           argv[1], enc_range);

  curl_free(enc_range);

  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
  headers = curl_slist_append(headers, auth_header);

  mem_init(&body);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

  // SSL verify ON
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  // timeout
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "Errore curl: %s\n", curl_easy_strerror(res));
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 2;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
  if (http < 200 || http >= 300) {
    fprintf(stderr, "HTTP %ld\n", http);
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 3;
  }

  // OK
  printf("%s\n", body.ptr ? body.ptr : "");

  free(body.ptr);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return 0;
}
