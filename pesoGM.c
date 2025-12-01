#include "func.c"
#include "pesoGM.def"

int main(){
  char access_token[512],url[512],auth_header[512],*out,post[2000],*qs,*p1;
  FILE *fp;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers=NULL;

  printf("Content-Type: text/plain\r\n\r\n");
  qs=getenv("QUERY_STRING");
  if(!qs||qs[0]=='\0')return 0;
  p1=strtok(qs,",");
  if(strcmp(p1,PAR0)!=0)return 0;
  p1=strtok(NULL,",");

  fp=fopen("/home/www/data/google_access_token","r");
  if(!fp)return 0;
  if(!fgets(access_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  access_token[strcspn(access_token,"\n")]='\0';

  sprintf(post,"{ \"valueInputOption\": \"RAW\", \"data\": [{ \"range\": \"%s\", \"majorDimension\": \"%s\", \"values\": [[ %s ]] }] }",PAR2,PAR3,p1);
  sprintf(auth_header,"Authorization: Bearer %s",access_token);
  headers=curl_slist_append(headers,"Content-Type: application/json");
  headers=curl_slist_append(headers,auth_header);
  sprintf(url,"https://sheets.googleapis.com/v4/spreadsheets/%s/values:batchUpdate",PAR1);
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;
  printf("%s\n",out);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return 1;
}
