#include "func.c"

int main(int argc,char *argv[]){
  char access_token[512],url[512],auth_header[512],*out;
  FILE *fp;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers;

  fp=fopen("/home/www/data/google_access_token","r");
  if(!fp)return 0;
  if(!fgets(access_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  access_token[strcspn(access_token,"\n")]='\0';
  
  headers=NULL;
  newout=1;
  sprintf(auth_header,"Authorization: Bearer %s",access_token);
  headers=curl_slist_append(headers,auth_header);
  sprintf(url,"https://www.googleapis.com/drive/v3/files?q=name='%s' and '%s' in parents&fields=files(id,name)",curl_easy_escape(curl,argv[1],0),curl_easy_escape(curl,argv[2],0));
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
 char errbuf[1000]; curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  res=curl_easy_perform(curl);
  printf("---%s\n%s\n",errbuf,url);

  if(res!=CURLE_OK)return 0;
    printf("2\n");
  printf("%s\n",out);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return 1;
}
