#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "/home/www/data/google_set.def"
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
