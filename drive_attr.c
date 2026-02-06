/*
 * Cerca un file su Google Drive per:
 *  - name = argv[1]
 *  - parent folder id = argv[2]
 * e stampa SOLO attributi (nessun download).
 *
 * Exit code:
 *  0 = trovato (1 solo file)
 *  2 = non trovato
 *  3 = ambiguita' (>=2 risultati)
 *  4 = parsing error / risposta inattesa
 *  5 = HTTP error (non 200)
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

/* URL-encode usando curl_easy_escape */
static char *urlenc(CURL *curl, const char *s) {
  if (!s) return NULL;
  return curl_easy_escape(curl, s, 0);
}

static int http_get(CURL *curl, const char *url, struct curl_slist *headers,
                    struct mem *out, long *http_code) {
  CURLcode res;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

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

/*
 * Conta quanti oggetti file ci sono in "files":[{...},{...}]
 * e restituisce un puntatore all'inizio del primo oggetto '{' (se presente).
 */
static int count_files_and_first_obj(const char *json, const char **first_obj) {
  if (first_obj) *first_obj = NULL;
  if (!json) return 0;

  const char *files = strstr(json, "\"files\"");
  if (!files) return 0;

  const char *arr = strchr(files, '[');
  if (!arr) return 0;

  // trova primo '{'
  const char *p = strchr(arr, '{');
  if (!p) return 0;
  if (first_obj) *first_obj = p;

  // conta occorrenze di '{' finché non chiude l'array ']'
  int count = 0;
  const char *end = strchr(arr, ']');
  if (!end) end = json + strlen(json);

  const char *q = arr;
  while ((q = strchr(q, '{')) && q < end) {
    count++;
    q++;
    if (count >= 2) break; // noi chiediamo pageSize=2
  }
  return count;
}

/*
 * Estrae un valore stringa semplice per una chiave JSON dentro un singolo oggetto:
 * "key":"VALUE"
 * Ritorna 1 se trovato, 0 se non trovato.
 */
static int json_get_string_field(const char *obj, const char *key, char *out, size_t outsz) {
  if (!obj || !key || !out || outsz == 0) return 0;
  char pat[256];
  snprintf(pat, sizeof(pat), "\"%s\"", key);

  const char *p = strstr(obj, pat);
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;

  // salta spazi
  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;

  if (*p != '"') return 0;
  p++; // dopo "

  const char *e = strchr(p, '"');
  if (!e) return 0;

  size_t n = (size_t)(e - p);
  if (n + 1 > outsz) n = outsz - 1;
  memcpy(out, p, n);
  out[n] = '\0';
  return 1;
}

/*
 * Estrae un valore numerico come stringa (es. "size": "1234" oppure "size": 1234).
 * Restituisce 1 se trovato.
 */
static int json_get_numberish_field(const char *obj, const char *key, char *out, size_t outsz) {
  if (!obj || !key || !out || outsz == 0) return 0;
  char pat[256];
  snprintf(pat, sizeof(pat), "\"%s\"", key);

  const char *p = strstr(obj, pat);
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;

  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;

  // può essere "123" o 123
  if (*p == '"') p++;

  const char *e = p;
  while (*e && *e != '"' && *e != ',' && *e != '}' && *e != '\n' && *e != '\r') e++;

  size_t n = (size_t)(e - p);
  if (n == 0) return 0;
  if (n + 1 > outsz) n = outsz - 1;
  memcpy(out, p, n);
  out[n] = '\0';
  return 1;
}

int main(int argc, char *argv[]) {
  char access_token[512];
  char auth_header[1024];

  char url[4096];
  char query[2048];

  CURL *curl = NULL;
  struct curl_slist *headers = NULL;

  struct mem body;
  long http = 0;

  if (argc != 3) {
    fprintf(stderr, "Uso: %s \"NOMEFILE\" \"PARENT_FOLDER_ID\"\n", argv[0]);
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

  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
  headers = curl_slist_append(headers, auth_header);

  char *esc_name = urlenc(curl, argv[1]);
  char *esc_parent = urlenc(curl, argv[2]);
  if (!esc_name || !esc_parent) {
    fprintf(stderr, "Errore: urlenc fallita\n");
    curl_free(esc_name);
    curl_free(esc_parent);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 1;
  }

  snprintf(query, sizeof(query),
           "name='%s' and '%s' in parents and trashed=false",
           esc_name, esc_parent);

  curl_free(esc_name);
  curl_free(esc_parent);

  char *esc_q = urlenc(curl, query);
  if (!esc_q) {
    fprintf(stderr, "Errore: urlenc(query) fallita\n");
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 1;
  }

  // fields ridotti: prendiamo solo quello che serve
  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files"
           "?q=%s"
           "&fields=files(id,name,mimeType,size,modifiedTime,md5Checksum,sha1Checksum,sha256Checksum,parents)"
           "&pageSize=2"
           "&supportsAllDrives=true"
           "&includeItemsFromAllDrives=true"
           "&corpora=allDrives",
           esc_q);

  curl_free(esc_q);

  mem_init(&body);
  if (!http_get(curl, url, headers, &body, &http)) {
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 1;
  }

  if (http != 200) {
    fprintf(stderr, "files.list HTTP %ld\n", http);
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 5;
  }

  const char *first_obj = NULL;
  int count = count_files_and_first_obj(body.ptr, &first_obj);

  if (count == 0) {
    // non trovato
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 2;
  }
  if (count >= 2) {
    // ambiguità
    fprintf(stderr, "Ambiguita': trovati >= 2 file con lo stesso name nel parent.\n");
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 3;
  }

  if (!first_obj) {
    fprintf(stderr, "Errore parsing JSON: oggetto file non trovato.\n");
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 4;
  }

  // Estrai attributi del primo (unico) file
  char id[256] = {0};
  char name[512] = {0};
  char mime[256] = {0};
  char size[64] = {0};
  char mtime[128] = {0};
  char md5[128] = {0};
  char sha1[128] = {0};
  char sha256[128] = {0};

  (void)json_get_string_field(first_obj, "id", id, sizeof(id));
  (void)json_get_string_field(first_obj, "name", name, sizeof(name));
  (void)json_get_string_field(first_obj, "mimeType", mime, sizeof(mime));
  (void)json_get_numberish_field(first_obj, "size", size, sizeof(size));
  (void)json_get_string_field(first_obj, "modifiedTime", mtime, sizeof(mtime));
  (void)json_get_string_field(first_obj, "md5Checksum", md5, sizeof(md5));
  (void)json_get_string_field(first_obj, "sha1Checksum", sha1, sizeof(sha1));
  (void)json_get_string_field(first_obj, "sha256Checksum", sha256, sizeof(sha256));

  // Output "pulito" (facile da parsare da shell)
  // Se un campo manca, rimane vuoto.
  printf("id=%s\n", id);
  printf("name=%s\n", name);
  printf("mimeType=%s\n", mime);
  printf("size=%s\n", size);
  printf("modifiedTime=%s\n", mtime);
  printf("md5Checksum=%s\n", md5);
  printf("sha1Checksum=%s\n", sha1);
  printf("sha256Checksum=%s\n", sha256);

  free(body.ptr);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return 0;
}
