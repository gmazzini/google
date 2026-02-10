#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#include "/home/www/data/google_set.def"   /* client_id, client_secret */

#define REFRESH_PATH "/home/www/data/google_refresh_token"
#define ACCESS_PATH  "/home/www/data/google_access_token"

/* -------- buffer dinamico per risposta curl -------- */
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

/* -------- estrazione minimale JSON "access_token":"..." -------- */
static int json_get_string(const char *json, const char *key, char *dst, size_t dst_len) {
  if (!json || !key || !dst || dst_len == 0) return -1;

  char pat[128];
  int n = snprintf(pat, sizeof(pat), "\"%s\"", key);
  if (n < 0 || (size_t)n >= sizeof(pat)) return -1;

  const char *p = strstr(json, pat);
  if (!p) return -1;
  p += strlen(pat);

  while (*p && isspace((unsigned char)*p)) p++;
  if (*p != ':') return -1;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;

  if (*p != '"') return -1;
  p++;

  size_t di = 0;
  while (*p) {
    if (*p == '\\' && p[1]) { /* gestione minimale escape */
      if (di + 1 >= dst_len) return -1;
      dst[di++] = p[1];
      p += 2;
      continue;
    }
    if (*p == '"') break;
    if (di + 1 >= dst_len) return -1;
    dst[di++] = *p++;
  }
  if (*p != '"') return -1;

  dst[di] = '\0';
  return 0;
}

static int write_file_line(const char *path, const char *s) {
  FILE *fp = fopen(path, "w");
  if (!fp) return -1;
  fprintf(fp, "%s\n", s);
  fclose(fp);
  return 0;
}

int main(void) {
  char refresh_token[2048];

  FILE *fp = fopen(REFRESH_PATH, "r");
  if (!fp) return 0;

  if (!fgets(refresh_token, sizeof(refresh_token), fp)) {
    fclose(fp);
    return 0;
  }
  fclose(fp);
  refresh_token[strcspn(refresh_token, "\r\n")] = '\0';
  if (refresh_token[0] == '\0') return 0;

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 0;

  CURL *curl = curl_easy_init();
  if (!curl) {
    curl_global_cleanup();
    return 0;
  }

  /* form-encoding corretto */
  char *e_client_id     = curl_easy_escape(curl, client_id, 0);
  char *e_client_secret = curl_easy_escape(curl, client_secret, 0);
  char *e_refresh       = curl_easy_escape(curl, refresh_token, 0);

  if (!e_client_id || !e_client_secret || !e_refresh) {
    curl_free(e_client_id);
    curl_free(e_client_secret);
    curl_free(e_refresh);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }

  char post[8192];
  int pn = snprintf(
    post, sizeof(post),
    "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
    e_client_id, e_client_secret, e_refresh
  );

  curl_free(e_client_id);
  curl_free(e_client_secret);
  curl_free(e_refresh);

  if (pn < 0 || (size_t)pn >= sizeof(post)) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }

  struct mem out = {0};
  long http_code = 0;

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
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

  /* Se ti serve debug */
  /* printf("%s\n", out.buf ? out.buf : ""); */

  if (http_code < 200 || http_code >= 300) {
    /* puoi stampare out.buf per vedere l'errore JSON */
    free(out.buf);
    return 0;
  }

  char access_token[2048];
  if (!out.buf || json_get_string(out.buf, "access_token", access_token, sizeof(access_token)) != 0) {
    free(out.buf);
    return 0;
  }

  if (write_file_line(ACCESS_PATH, access_token) != 0) {
    free(out.buf);
    return 0;
  }

  free(out.buf);
  return 1;
}
