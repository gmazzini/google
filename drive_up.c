/*
 * Upload Google Drive (v3) in 2 step:
 *  1) POST uploadType=media -> crea file binario e ottiene id
 *  2) PATCH files/{id}?addParents=... -> set name + parent
 *
 * Migliorie rispetto al vecchio:
 * - SSL verify ON (CURLOPT_SSL_VERIFYPEER/VERIFYHOST)
 * - timeout ragionevoli
 * - streaming del file (non lo carica tutto in RAM)
 * - gestione errori: stampa HTTP code + body
 * - supportsAllDrives=true
 * - parsing JSON per id più robusto
 *
 * Requisiti:
 * - access token in /home/www/data/google_access_token (una riga)
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

static size_t read_file_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
  FILE *fp = (FILE*)userdata;
  return fread(buffer, size, nitems, fp);
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

/* mime minimale, puoi sostituire con la tua mime() se vuoi */
static const char *mime_from_name(const char *name) {
  const char *ext = strrchr(name, '.');
  if (!ext) return "application/octet-stream";
  ext++;

  if (!strcasecmp(ext, "mp3")) return "audio/mpeg";
  if (!strcasecmp(ext, "wav")) return "audio/wav";
  if (!strcasecmp(ext, "txt")) return "text/plain";
  if (!strcasecmp(ext, "json")) return "application/json";
  if (!strcasecmp(ext, "pdf")) return "application/pdf";
  if (!strcasecmp(ext, "jpg") || !strcasecmp(ext, "jpeg")) return "image/jpeg";
  if (!strcasecmp(ext, "png")) return "image/png";
  return "application/octet-stream";
}

/* Estrae "id":"...." dal JSON (prima occorrenza) */
static int extract_id(const char *json, char *out_id, size_t outsz) {
  if (!json || !out_id || outsz == 0) return 0;

  const char *p = strstr(json, "\"id\"");
  if (!p) return 0;

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

  memcpy(out_id, p, n);
  out_id[n] = '\0';
  return 1;
}

static int http_do(CURL *curl, const char *url, struct curl_slist *headers,
                   struct mem *out, long *http_code) {
  CURLcode res;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

  // SSL verify ON
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  // timeout
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "Errore curl: %s\n", curl_easy_strerror(res));
    return 0;
  }

  if (http_code) {
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    *http_code = code;
  }
  return 1;
}

static int upload_media(CURL *curl, struct curl_slist *headers,
                        FILE *fp, curl_off_t filesize,
                        char *out_file_id, size_t outsz) {
  char url[512];
  struct mem body;
  long http = 0;

  snprintf(url, sizeof(url),
           "https://www.googleapis.com/upload/drive/v3/files"
           "?uploadType=media"
           "&supportsAllDrives=true"
           "&fields=id");

  mem_init(&body);

  curl_easy_reset(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // POST con body letto da FILE*
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_cb);
  curl_easy_setopt(curl, CURLOPT_READDATA, fp);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, filesize);

  // output body
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

  // SSL + timeout
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "upload media curl error: %s\n", curl_easy_strerror(res));
    free(body.ptr);
    return 0;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
  if (http < 200 || http >= 300) {
    fprintf(stderr, "upload media HTTP %ld\n", http);
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    return 0;
  }

  if (!extract_id(body.ptr, out_file_id, outsz)) {
    fprintf(stderr, "Errore: impossibile estrarre id dal JSON.\n");
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    return 0;
  }

  free(body.ptr);
  return 1;
}

static int patch_metadata(CURL *curl, struct curl_slist *headers,
                          const char *file_id, const char *parent_id,
                          const char *remote_name) {
  char url[1024];
  char json[2048];
  struct mem body;
  long http = 0;

  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files/%s"
           "?addParents=%s"
           "&supportsAllDrives=true"
           "&fields=id,name,parents",
           file_id, parent_id);

  // JSON minimal: set name
  // (se remote_name contiene " speciali, dovresti fare escaping JSON)
  snprintf(json, sizeof(json), "{ \"name\": \"%s\" }", remote_name);

  mem_init(&body);

  curl_easy_reset(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json));

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "patch curl error: %s\n", curl_easy_strerror(res));
    free(body.ptr);
    return 0;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
  if (http < 200 || http >= 300) {
    fprintf(stderr, "patch HTTP %ld\n", http);
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    return 0;
  }

  // opzionale: stampa risposta patch (debug)
  // printf("%s\n", body.ptr ? body.ptr : "");

  free(body.ptr);
  return 1;
}

int main(int argc, char *argv[]) {
  char access_token[512];
  char auth_header[1024];
  char ct_header[256];
  char file_id[256];

  CURL *curl = NULL;
  struct curl_slist *headers_upload = NULL;
  struct curl_slist *headers_patch = NULL;

  if (argc != 4) {
    fprintf(stderr, "Uso: %s LOCAL_FILE PARENT_FOLDER_ID REMOTE_NAME\n", argv[0]);
    return 1;
  }

  if (!read_access_token(access_token, sizeof(access_token))) {
    return 1;
  }

  FILE *fp = fopen(argv[1], "rb");
  if (!fp) {
    fprintf(stderr, "Errore: impossibile aprire file %s\n", argv[1]);
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long fs_long = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (fs_long < 0) {
    fprintf(stderr, "Errore: ftell fallita\n");
    fclose(fp);
    return 1;
  }
  curl_off_t fs = (curl_off_t)fs_long;

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    fprintf(stderr, "Errore: curl_global_init fallita\n");
    fclose(fp);
    return 1;
  }

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Errore: curl_easy_init fallita\n");
    fclose(fp);
    curl_global_cleanup();
    return 1;
  }

  // Authorization header
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);

  // ---- headers per upload (Content-Type + Authorization)
  snprintf(ct_header, sizeof(ct_header), "Content-Type: %s", mime_from_name(argv[3]));
  headers_upload = curl_slist_append(headers_upload, ct_header);
  headers_upload = curl_slist_append(headers_upload, auth_header);

  // ---- step 1: upload media -> id
  if (!upload_media(curl, headers_upload, fp, fs, file_id, sizeof(file_id))) {
    fclose(fp);
    curl_slist_free_all(headers_upload);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 2;
  }
  fclose(fp);

  // ---- headers per patch (JSON + Authorization)
  headers_patch = curl_slist_append(headers_patch, "Content-Type: application/json");
  headers_patch = curl_slist_append(headers_patch, auth_header);

  // ---- step 2: patch name + addParents
  if (!patch_metadata(curl, headers_patch, file_id, argv[2], argv[3])) {
    fprintf(stderr, "Upload creato ma patch fallita. id=%s\n", file_id);
    curl_slist_free_all(headers_upload);
    curl_slist_free_all(headers_patch);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 3;
  }

  printf("OK: caricato %s -> id=%s (parent=%s name=%s)\n", argv[1], file_id, argv[2], argv[3]);

  curl_slist_free_all(headers_upload);
  curl_slist_free_all(headers_patch);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return 0;
}
