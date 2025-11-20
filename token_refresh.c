#include "func.c"

int main(){
  char refresh_token[512],*access_token,post[100],tok[30],*out,*p1,*p2;
  FILE *fp;
  CURL *curl;
  CURLcode res;
  int c;
  
  fp=fopen("/home/www/data/google_refresh_token","r");
  if(!fp)return 0;
  if(!fgets(refresh_token,512,fp)){fclose(fp); return 0;}
  fclose(fp);
  refresh_token[strcspn(refresh_token,"\n")]='\0';

printf("%s\n",refresh_token);
  
  sprintf(post,"client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",client_id,client_secret,refresh_token);
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,"https://oauth2.googleapis.com/token");
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;

printf("%s\n",out);
  
  strcpy(tok,"\"access_token\": \"");
  p1=strstr(out,tok);
  if(p1==NULL)return 0;
  access_token=p1+strlen(tok);
  p2=strstr(access_token,"\"");
  if(p2==NULL)return 0;
  c=*p2; *p2='\0';
  fp=fopen("/home/www/data/google_access_token","w");
  if(fp==NULL){curl_easy_cleanup(curl); return 0;}
  fprintf(fp,"%s\n",access_token);
  fclose(fp);
  *p2=c;

  curl_easy_cleanup(curl);
  return 1;
}
