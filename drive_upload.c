#include "func.c"

int main(int argc,char *argv[]){
  FILE *fp;
  char *buf,access_token[512],auth_header[512],url[512],*out,*p1,*p2,*id;
  long fs;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers;

  fp=fopen("/home/www/data/google_access_token","r");
  if(!fp)return 0;
  if(!fgets(access_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  access_token[strcspn(access_token,"\n")]='\0';
  
  fp=fopen(argv[1],"rb");
  if(fp==NULL)return 0;
  fseek(fp,0,SEEK_END);
  fs=ftell(fp);
  buf=(char *)malloc(fs*sizeof(char));
  if(buf==NULL){fclose(fp); return 0;}
  fread(buf,1,fs,fp);
  fclose(fp);

  headers=NULL;
  sprintf(auth_header,"Authorization: Bearer %s",access_token);
  headers=curl_slist_append(headers,auth_header);
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,"https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,buf);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;
  printf("%s\n",out);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  strcpy(tok,"\"id\": \"");
  p1=strstr(out,tok);
  if(p1==NULL)return 0;
  id=p1+strlen(tok);
  p2=strstr(access_token,"\"");
  if(p2==NULL)return 0;
  *p2='\0';

  headers=NULL;
  sprintf(auth_header,"Authorization: Bearer %s",access_token);
  sprintf(buf,"{ \"name\": \"pippo\" }");
  headers=curl_slist_append(headers,"Content-Type: application/json");
  headers=curl_slist_append(headers,auth_header);
  curl=curl_easy_init();
  if(!curl)return 0;
  sprintf(url,"https://www.googleapis.com/drive/v3/files/%s?addParents=%s",id,argv[2]);
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,"PATCH");
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,buf);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;
  printf("%s\n",out);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl); 
  
  return 1;
  
}
