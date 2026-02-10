#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#include "/home/www/data/google_set.def"   /* deve definire: client_id, client_secret, redirect_uri */

/* Dove salvare i token */
#define ACCESS_PATH  "/home/www/data/google_access_token"
#define REFRESH_PATH "/home/www/data/google_refresh_token"

/* -------------------- buffer dinamico per risposta curl -------------------- */
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

/* -------------------- util: decode querystring + estrai parametro -------------------- */
static int hexval(int c) {
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'f') return c - 'a' + 10;
  if ('A' <= c && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int url_decode(const char *src, char *dst, size_t dst_len) {
  size_t di = 0;
  for (size_t si = 0; src[si]; si++) {
    if (di + 1 >= dst_len) return -1;

    if (src[si] == '+') {
      dst[di++] = ' ';
    } else if (src[si] == '%' &&
               isxdigit((unsigned char)src[si+1]) &&
               isxdigit((unsigned char)src[si+2])) {
      int hi = hexval(src[si+1]);
      int lo = hexval(src[si+2]);
      if (hi < 0 || lo < 0) return -1;
      dst[di++] = (char)((hi << 4) | lo);
      si += 2;
    } else {
      dst[di++] = src[si];
    }
  }
  dst[di] = '\0';
  return 0;
}

static int get_qs_param(const char *qs, const char *key, char *out, size_t out_len) {
  if (!qs || !key || !out || out_len == 0) return -1;

  size_t klen = strlen(key);
  const char *p = qs;

  while (*p) {
    const char *amp = strchr(p, '&');
    size_t seglen = amp ? (size_t)(amp - p) : strlen(p);

    if (seglen > klen + 1 && strncmp(p, key, klen) == 0 && p[klen] == '=') {
      const char *v = p + klen + 1;
      size_t vlen = seglen - (klen + 1);

      if (vlen >= 4096) return -1; /* limite di sicurezza */
      char tmp[4096];
      memcpy(tmp, v, vlen);
      tmp[vlen] = '\0';
      return url_decode(tmp, out, out_len);
    }

    if (!amp) break;
    p = amp + 1;
  }
  return -1;
}

/* -------------------- util: estrai stringa JSON "key":"value" (minimale) -------------------- */
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

/* -------------------- main CGI -------------------- */
int main(void) {
  printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");

  const char *method = getenv("REQUEST_METHOD");
  if (!method || strcmp(method, "GET") != 0) {
    printf("Bad method\n");
    return 0;
  }

  const char *qs = getenv("QUERY_STRING");
  if (!qs) {
    printf("No query\n");
    return 0;
  }

  char code[2048];
  if (get_qs_param(qs, "code", code, sizeof(code)) != 0) {
    printf("Missing code\n");
    return 0;
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    printf("curl_global_init failed\n");
    return 0;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    curl_global_cleanup();
    printf("curl_easy_init failed\n");
    return 0;
  }

  /* form-encoding corretto per application/x-www-form-urlencoded */
  char *e_client_id     = curl_easy_escape(curl, client_id, 0);
  char *e_redirect_uri  = curl_easy_escape(curl, redirect_uri, 0);
  char *e_client_secret = curl_easy_escape(curl, client_secret, 0);
  char *e_code          = curl_easy_escape(curl, code, 0);

  if (!e_client_id || !e_redirect_uri || !e_client_secret || !e_code) {
    printf("escape failed\n");
    curl_free(e_client_id); curl_free(e_redirect_uri);
    curl_free(e_client_secret); curl_free(e_code);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
  }

  char post[4096];
  int pn = snprintf(
    post, sizeof(post),
    "client_id=%s&redirect_uri=%s&client_secret=%s&code=%s&grant_type=authorization_code",
    e_client_id, e_redirect_uri, e_client_secret, e_code
  );

  curl_free(e_client_id);
  curl_free(e_redirect_uri);
  curl_free(e_client_secret);
  curl_free(e_code);

  if (pn < 0 || (size_t)pn >= sizeof(post)) {
    printf("POST too long\n");
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
    printf("curl error: %s\n", curl_easy_strerror(res));
    free(out.buf);
    return 0;
  }

  if (http_code < 200 || http_code >= 300) {
    printf("HTTP %ld\n", http_code);
    if (out.buf) printf("%s\n", out.buf);
    free(out.buf);
    return 0;
  }

  char access_token[2048];
  if (json_get_string(out.buf, "access_token", access_token, sizeof(access_token)) != 0) {
    printf("No access_token\n");
    if (out.buf) printf("%s\n", out.buf);
    free(out.buf);
    return 0;
  }

  if (write_file_line(ACCESS_PATH, access_token) != 0) {
    printf("Cannot write access token\n");
    free(out.buf);
    return 0;
  }

  /* refresh_token: può mancare */
  char refresh_token[2048];
  if (json_get_string(out.buf, "refresh_token", refresh_token, sizeof(refresh_token)) == 0) {
    (void)write_file_line(REFRESH_PATH, refresh_token);
  }

  printf("OK\n");
  free(out.buf);
  return 1;
}

