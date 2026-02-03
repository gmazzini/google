/*
 * gdrive_get_mp3.c
 *
 * Scarica un file (es. MP3) da Google Drive cercandolo per:
 *   - name = argv[1]
 *   - parent folder id = argv[2]
 * e salvandolo in argv[3].
 *
 * Fix principali rispetto al tuo:
 * - curl_easy_escape() usato SOLO dopo curl_easy_init()
 * - gestione Shared Drives: supportsAllDrives/includeItemsFromAllDrives/corpora=allDrives
 * - parsing JSON più robusto e limitazione fields/pageSize
 * - stampa HTTP code + body in caso di errore
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -o gdrive_get_mp3 gdrive_get_mp3.c -lcurl
 *
 * Uso:
 *   ./gdrive_get_mp3 "file.mp3" "PARENT_FOLDER_ID" "/tmp/file.mp3"
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

/* URL-encode generico usando curl_easy_escape */
static char *urlenc(CURL *curl, const char *s) {
  if (!s) return NULL;
  return curl_easy_escape(curl, s, 0);
}

/*
 * Estrae l'ID dal JSON di files.list.
 * Ci aspettiamo risposta con fields=files(id,name) e pageSize=2.
 * Ritorna:
 *   1 se trovato esattamente 1 file e copia in out_id
 *   0 se nessun file
 *  -1 se più di un file (ambiguità)
 *  -2 parsing error
 */
static int extract_single_id(const char *json, char *out_id, size_t outsz) {
  const char *p = json;
  int count = 0;
  const char *first_id_start = NULL;
  const char *first_id_end = NULL;

  if (!json) return -2;

  /* conta occorrenze di "id": "...." ma SOLO dentro la sezione files */
  const char *files = strstr(json, "\"files\"");
  if (!files) return 0;

  p = files;
  while (1) {
    const char *idk = strstr(p, "\"id\"");
    if (!idk) break;

    /* trova : " */
    const char *colon = strchr(idk, ':');
    if (!colon) { p = idk + 4; continue; }

    const char *q1 = strchr(colon, '"');
    if (!q1) { p = colon + 1; continue; }
    q1++; /* dopo il primo " */

    const char *q2 = strchr(q1, '"');
    if (!q2) { p = q1; continue; }

    count++;
    if (count == 1) {
      first_id_start = q1;
      first_id_end = q2;
    }
    p = q2 + 1;
    if (count >= 2) break; /* noi chiediamo max 2 */
  }

  if (count == 0) return 0;
  if (count > 1) return -1;

  size_t n = (size_t)(first_id_end - first_id_start);
  if (n + 1 > outsz) return -2;
  memcpy(out_id, first_id_start, n);
  out_id[n] = '\0';
  return 1;
}

static int http_get(CURL *curl, const char *url, struct curl_slist *headers, struct mem *out, long *http_code) {
  CURLcode res;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

  /* Sicurezza: lascia SSL verify ON di default */
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  /* timeout ragionevoli */
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

int main(int argc, char *argv[]) {
  char access_token[512];
  char auth_header[1024];

  char url[4096];
  char query[2048];
  char file_id[256];

  CURL *curl = NULL;
  struct curl_slist *headers = NULL;

  struct mem body;
  long http = 0;

  if (argc != 4) {
    fprintf(stderr, "Uso: %s \"NOMEFILE\" \"PARENT_FOLDER_ID\" \"OUTPUT_PATH\"\n", argv[0]);
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

  /* header Authorization */
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
  headers = curl_slist_append(headers, auth_header);

  /* ---------- 1) files.list: cerca ID per name + parent ---------- */
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

  /* encode query come parametro q=... */
  char *esc_q = urlenc(curl, query);
  if (!esc_q) {
    fprintf(stderr, "Errore: urlenc(query) fallita\n");
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 1;
  }

  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files"
           "?q=%s"
           "&fields=files(id,name,mimeType)"
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
    return 1;
  }

  int idres = extract_single_id(body.ptr, file_id, sizeof(file_id));
  if (idres == 0) {
    fprintf(stderr, "Nessun file trovato con name='%s' in parent='%s'\n", argv[1], argv[2]);
    /* utile per debug: */
    fprintf(stderr, "Risposta: %s\n", body.ptr);
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 2;
  }
  if (idres == -1) {
    fprintf(stderr, "Trovati piu' file con lo stesso nome (ambiguita').\n");
    fprintf(stderr, "Risposta: %s\n", body.ptr);
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 3;
  }
  if (idres < 0) {
    fprintf(stderr, "Errore parsing JSON.\n");
    fprintf(stderr, "Risposta: %s\n", body.ptr);
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 4;
  }

  free(body.ptr);

  /* ---------- 2) files.get alt=media: download binario ---------- */
  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files/%s"
           "?alt=media"
           "&supportsAllDrives=true",
           file_id);

  mem_init(&body);
  if (!http_get(curl, url, headers, &body, &http)) {
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 1;
  }

  if (http != 200) {
    fprintf(stderr, "files.get HTTP %ld\n", http);
    fprintf(stderr, "BODY: %s\n", body.ptr ? body.ptr : "(null)");
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 5;
  }

  FILE *fp = fopen(argv[3], "wb");
  if (!fp) {
    fprintf(stderr, "Errore: impossibile aprire output %s\n", argv[3]);
    free(body.ptr);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 6;
  }

  if (body.len > 0) {
    fwrite(body.ptr, 1, body.len, fp);
  }
  fclose(fp);

  printf("OK: scaricato '%s' (id=%s) in %s (%zu bytes)\n", argv[1], file_id, argv[3], body.len);

  free(body.ptr);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();

  return 0;
}
