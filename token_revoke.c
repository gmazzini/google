#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "/home/www/data/google_set.def" /* opzionale qui */

#define ACCESS_PATH "/home/www/data/google_access_token"

/* buffer dinamico (anche se la risposta è piccola, è comodo per debug) */
struct mem {
  char *buf;
  size_t len;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t realsize = size * nmemb;
  struct mem *m = (struct mem *)userdata;

  char *p = realloc(m->buf, m->len + realsize + 1);
  if (!p) return 0;

  m->buf = p;
  memcpy(m->buf + m->len, ptr, realsize);
  m->len += realsize;
  m->buf[m->len] = '\0';
  return realsize;
}

int main(void) {
  char access_token[2048];

  FILE *fp = fopen(ACCESS_PATH, "r");
  if (!fp) return 0;

  if (!fgets(access_token, sizeof(access_token), fp)) {
    fclose(fp);
    return 0;
  }
  fclose(fp);

  access_token[strcspn(access_token, "\r\n")] = '\0';
  if (access_token[0] == '\0') return 0;

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 0;

  CURL *curl = curl_easy_init();
  if (!curl) {
    curl_global_cleanup();
    return 0;
  }

  /* form-encoding corretto */
  char *e_tok = curl_easy_escape(curl, access_token, 0);
  if (!e_tok) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }

  char post[4096];
  int pn = snprintf(post, sizeof(post), "token=%s", e_tok);
  curl_free(e_tok);

  if (pn < 0 || (size_t)pn >= sizeof(post)) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }

  struct mem out = {0};
  long http_code = 0;

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/revoke");
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

  /* TLS verificato */
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

  CURLcode res = curl_easy_perform(curl);
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  if (res != CURLE_OK) {
    free(out.buf);
    return 0;
  }

  /* Google revoke: 200 = ok, 400 = token già revocato/invalid → dipende da te se considerarlo ok */
  if (http_code == 200) {
    free(out.buf);
    return 1;
  }

  /* Se vuoi trattare 400 come “già revocato”, scommenta:
     if (http_code == 400) { free(out.buf); return 1; }
  */

  /* debug opzionale:
     if (out.buf) fprintf(stderr, "HTTP %ld: %s\n", http_code, out.buf);
  */
  free(out.buf);
  return 0;
}
