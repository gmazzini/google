#include "func.c"

int main(){
  char *in,*p1,*p2,*access_token,*refresh_token,url[512],post[512],tok[30],*out;
  CURL *curl;
  CURLcode res;
  FILE *fp;
  int c;

  printf("Content-Type: text/plain\r\n\r\n");
  if(strcmp(getenv("REQUEST_METHOD"),"GET")!=0)return 0;
  in=getenv("QUERY_STRING");
  p2=strstr(in,"code=");
  if(p2==NULL)return 0;
  p2+=5;
  p1=p2;
  while(*p1!='\0' && *p1!='&' && *p1!='\n')p1++;
  *p1='\0';
  sprintf(post,"client_id=%s&redirect_uri=%s&client_secret=%s&code=%s&access_type=offline&grant_type=authorization_code",client_id,redirect_uri,client_secret,p2);
  sprintf(url,"https://oauth2.googleapis.com/token");
  curl=curl_easy_init();
  if(!curl)return 0;
  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb2);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out);
  curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
  res=curl_easy_perform(curl);
  if(res!=CURLE_OK)return 0;


 fp=fopen("/home/www/data/q1","w");
  if(fp==NULL){curl_easy_cleanup(curl); return 0;}
  fprintf(fp,"%s\n",out);
  fclose(fp);

  
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
  
  strcpy(tok,"\"refresh_token\": \"");
  p1=strstr(out,tok);
  if(p1==NULL)return 0;
  refresh_token=p1+strlen(tok);
  p2=strstr(access_token,"\"");
  if(p2==NULL)return 0;
  c=*p2; *p2='\0';
  fp=fopen("/home/www/data/google_refresh_token","w");
  if(fp==NULL){curl_easy_cleanup(curl); return 0;}
  fprintf(fp,"%s\n",refresh_token);
  fclose(fp);
  *p2=c;
  
  curl_easy_cleanup(curl);
  return 1;
}
