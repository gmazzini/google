#include "func.c"

int main(int argc,char *argv[]){
  char access_token[512],url[512],auth_header[512],query[512],tok[30],*p1,*p2,*id,*out;
  FILE *fp;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers;
  int i;

  fp=fopen("/home/www/data/google_access_token","r");
  if(!fp)return 0;
  if(!fgets(access_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  access_token[strcspn(access_token,"\n")]='\0';
  
  headers=NULL;
  newout=1;
  sprintf(auth_header,"Authorization: Bearer %s",access_token);
  headers=curl_slist_append(headers,auth_header);
  sprintf(query,"name='%s' and '%s' in parents",curl_easy_escape(curl,argv[1],0),curl_easy_escape(curl,argv[2],0));
  sprintf(url,"https://www.googleapis.com/drive/v3/files?q=%s",myencode(query));
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;
  printf("%s\n",out);

  for(i=0;;i++){
    strcpy(tok,"\"id\": \"");
    p1=strstr(out,tok);
    if(p1==NULL)break;
    id=p1+strlen(tok);
    p2=strstr(id,"\"");
    if(p2==NULL)break;
  }
  if(i!=1)return 0;
  *p2='\0';

printf("%s\n",id);
  
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return 1;
}
