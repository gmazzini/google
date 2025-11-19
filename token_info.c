#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>


struct memory {
    char *response;
    size_t size;
};

/* libcurl richiede per forza una callback */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL) {
        return 0;  // errore allocazione
    }

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}


int main(){
  char access_token[512],url[512];
  FILE *fp;
  CURL *curl;
  CURLcode res;
  struct memory chunk;


  fp=fopen("/home/www/data/access_token","r");
  if(!fp)return 0;
  if(!fgets(access_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  access_token[strcspn(access_token,"\n")]='\0';
  sprintf(url,"https://oauth2.googleapis.com/tokeninfo?access_token=%s",access_token);

    /* Inizializza libcurl */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "Errore: impossibile inizializzare curl\n");
        return 1;
    }

    chunk.response = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    /* Evita verifica SSL come nel tuo PHP */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    /* Esegui richiesta */
    res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
        fprintf(stderr, "Errore curl: %s\n", curl_easy_strerror(res));
    } else {
        printf("%s\n", chunk.response);
    }

    free(chunk.response);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return 0;
}
