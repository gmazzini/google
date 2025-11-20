#include "func.c"

int main(void) {
  char url[512];
  sprintf(url,"https://accounts.google.com/o/oauth2/auth?scope=%s&access_type=offline&response_type=code&redirect_uri=%s&client_id=%s",myencode(scope),redirect_uri,client_id);
  printf("Status: 302 Found\r\n");
  printf("Location: %s\r\n", googleOauthURL);
  printf("\r\n");
}
