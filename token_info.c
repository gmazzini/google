#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define BUFOUT 5000

int newout=1;
static size_t write_cb2(void *ptr,size_t size,size_t nmemb,void *userdata){
  size_t realsize=size*nmemb;
  char **buffer=(char **)userdata;
  static char *out=NULL;
  static size_t actpos=0;
  if(out==NULL){
    out=(char *)malloc(BUFOUT*sizeof(char));
    if(out==NULL)return 0;
  }
  if(newout){actpos=0; newout=0;}
  memcpy(out+actpos,ptr,realsize);
  actpos+=realsize;
  *(out+actpos)='\0'; 
  *buffer=out;
  return realsize;
}

int main(){
  char access_token[512],url[512],*out;
  FILE *fp;
  CURL *curl;
  CURLcode res;
  
  fp=fopen("/home/www/data/google_access_token","r");
  if(!fp)return 0;
  if(!fgets(access_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  access_token[strcspn(access_token,"\n")]='\0';
  sprintf(url,"https://oauth2.googleapis.com/tokeninfo?access_token=%s",access_token);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;
  printf("%s\n",out);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return 1;
}
