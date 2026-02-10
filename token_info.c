#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define TOKEN_PATH "/home/www/data/google_access_token"

struct mem {
    char *buf;
    size_t len;
};

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct mem *m = (struct mem *)userp;

    char *p = realloc(m->buf, m->len + realsize + 1);
    if (!p) return 0; // out of memory -> abort transfer

    m->buf = p;
    memcpy(&(m->buf[m->len]), contents, realsize);
    m->len += realsize;
    m->buf[m->len] = '\0';
    return realsize;
}

int main(void) {
    char access_token[512];
    char url[1024];
    FILE *fp = NULL;
    CURL *curl = NULL;
    CURLcode res;
    long http_code = 0;

    struct mem out = {0};

    fp = fopen(TOKEN_PATH, "r");
    if (!fp) return 0;

    if (!fgets(access_token, sizeof(access_token), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    access_token[strcspn(access_token, "\r\n")] = '\0';
    if (access_token[0] == '\0') return 0;

    int n = snprintf(
        url, sizeof(url),
        "https://oauth2.googleapis.com/tokeninfo?access_token=%s",
        access_token
    );
    if (n < 0 || (size_t)n >= sizeof(url)) return 0;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 0;

    curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

    /* Sicurezza TLS: NON disabilitare la verifica */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    /* facoltativo: timeout ragionevoli */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK) {
        free(out.buf);
        return 0;
    }

    if (http_code < 200 || http_code >= 300) {
        /* Se vuoi, qui puoi loggare out.buf per vedere l'errore JSON */
        free(out.buf);
        return 0;
    }

    /* out.buf contiene la risposta JSON (tokeninfo) */
    if (out.buf) {
        printf("%s\n", out.buf);
    }

    free(out.buf);
    return 1;
}
