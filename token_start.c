#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* CONFIG */
static const char scope[] =
    "openid email profile";
static const char redirect_uri[] =
    "https://example.com/callback";
static const char client_id[] =
    "YOUR_CLIENT_ID";

/* URL encode semplice (RFC 3986) */
static int url_encode(const char *src, char *dst, size_t dst_len) {
    static const char hex[] = "0123456789ABCDEF";
    size_t di = 0;

    for (size_t si = 0; src[si]; si++) {
        unsigned char c = (unsigned char)src[si];

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            if (di + 1 >= dst_len) return -1;
            dst[di++] = c;
        } else {
            if (di + 3 >= dst_len) return -1;
            dst[di++] = '%';
            dst[di++] = hex[c >> 4];
            dst[di++] = hex[c & 0x0F];
        }
    }

    if (di >= dst_len) return -1;
    dst[di] = '\0';
    return 0;
}

int main(void) {
    char enc_scope[256];
    char url[512];

    if (url_encode(scope, enc_scope, sizeof(enc_scope)) < 0) {
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");
        printf("Scope encoding failed\n");
        return 1;
    }

    int n = snprintf(
        url, sizeof(url),
        "https://accounts.google.com/o/oauth2/v2/auth"
        "?scope=%s"
        "&access_type=offline"
        "&response_type=code"
        "&redirect_uri=%s"
        "&client_id=%s",
        enc_scope, redirect_uri, client_id
    );

    if (n < 0 || (size_t)n >= sizeof(url)) {
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");
        printf("URL too long\n");
        return 1;
    }

    printf("Status: 302 Found\r\n");
    printf("Location: %s\r\n", url);
    printf("Cache-Control: no-store\r\n");
    printf("Pragma: no-cache\r\n");
    printf("Content-Type: text/plain; charset=utf-8\r\n");
    printf("\r\n");

    return 0;
}
