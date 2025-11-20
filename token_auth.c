#include "func.c"

int main(){
  char in[512],*p1,*p2,url[512],post[512],*out;
  CURL *curl;
  CURLcode res;
  int c;

  printf("Content-Type: text/plain\r\n\r\n");
  for(p1=in;;){
    c=getchar();
    if(c==EOF)break;
    *p1++=c;
  }
  *p1++='\0';
  p2=strstr(in,"code=");
  p2+=5;
  p1=p2;
  while(*p1 && *p1 != '&')p1++;
  p1='\0';
  FILE *fp2; fp2=fopen("/home/www/google/q2.txt","w");
  fprintf(fp2,"%s\n%s\n",in,p2);
  fclose(fp2);
  
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
  FILE *fp; fp=fopen("/home/www/google/q1.txt","w");
  fprintf(fp,"%s\n",out);
  fclose(fp);
  curl_easy_cleanup(curl);
  return 1;
}
