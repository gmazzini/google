#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "/home/www/data/google_set.def"
#define BUFOUT 5000000

int newout=1;
size_t actpos=0;
static size_t write_cb2(void *ptr,size_t size,size_t nmemb,void *userdata){
  size_t realsize=size*nmemb;
  char **buffer=(char **)userdata;
  static char *out=NULL;
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

char *myencode(char *pstr){
  static char enc[512];
  char *penc=enc;
  while(*pstr){
    if((*pstr >= 'a' && *pstr <= 'z') || (*pstr >= 'A' && *pstr <= 'Z') || (*pstr >= '0' && *pstr <= '9') || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')*penc++=*pstr;
    else {sprintf(penc,"%%%02X",*pstr); penc += 3;}
    pstr++;
  }
  *penc='\0';
  return enc;
}

char *mime(char *filename){
  char *ext;
  ext=strrchr(filename,'.');
  if(!ext || ext==filename)return "application/octet-stream";
  ext++;
  if(strcasecmp(ext,"pdf")==0)return "application/pdf";
  if(strcasecmp(ext,"png")==0)return "image/png";
  if(strcasecmp(ext,"jpg")==0 || strcasecmp(ext,"jpeg")==0)return "image/jpeg";
  if(strcasecmp(ext,"gif")==0)return "image/gif";
  if(strcasecmp(ext,"txt")==0)return "text/plain";
  if(strcasecmp(ext,"html")==0 || strcasecmp(ext,"htm")==0)return "text/html";
  if(strcasecmp(ext,"csv")==0)return "text/csv";
  if(strcasecmp(ext,"docx")==0)return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
  if(strcasecmp(ext,"xlsx")==0)return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
  if(strcasecmp(ext,"pptx")==0)return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
  if(strcasecmp(ext,"zip")==0)return "application/zip";
  return "application/octet-stream";
}
