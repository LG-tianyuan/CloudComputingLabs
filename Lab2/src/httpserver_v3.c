/*
 * @Author: LG.tianyuan
 * @Date: 2022-04-19
*/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pthread.h>
#include <getopt.h>
#include <regex.h>
#include <netdb.h>
#include <sys/epoll.h>
#include "threadpool.h"

#define MAXSIZE 2000

static const char *data_format1="^id=[0-9]+&name=[0-9a-zA-Z]+$";
static const char *data_format2="^\\{\"id\":\"[0-9]+\",\"name\":\"[0-9a-zA-Z]+\"\\}$";
static const char *data_format3="^(id=[0-9]+)|(name=[0-9a-zA-Z]+)|(id=[0-9]+&name=[0-9a-zA-Z]+)$";
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  char* name;
  int flag;
  char* content;
} FileInfo;

FileInfo* file_list;

typedef struct {
  char* psa;
  int cfd;
  int epfd;
} ConnectInfo;

typedef struct {
  int cfd_down;
  int cfd_up;
} cfdInfo;

void init()
{
  file_list = (FileInfo*)malloc(15*sizeof(FileInfo));
  for(int i=0;i<15;i++)
  {
    file_list[i].name = (char*)malloc(50);
    strcpy(file_list[i].name,"-1");
    file_list[i].content = (char*)malloc(2048);
  }
}

void program_end()
{
  for(int i=0;i<15;i++)
  {
    free(file_list[i].name);
    free(file_list[i].content);
  }
  free(file_list);
}

// 通过文件名获取文件的类型
char *get_file_type(const char *name)
{
    char* dot;
    dot = strrchr(name, '.');   //在字符串中搜索最后一次出现字符'.'的位置
    if(dot == NULL || strcmp(dot, ".txt") == 0)
    {
      return "text/plain";
    }    
    else if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
    {
      return "text/html";
    }
    else if(strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
    {
      return "image/jpeg";
    }
    else if(strcmp(dot, ".gif") == 0)
    {
      return "image/gif";
    }
    else if(strcmp(dot, ".png") == 0)
    {
      return "image/png";
    }
    else if(strcmp(dot, ".css") == 0)
    {
      return "text/css";
    }
    else if(strcmp(dot, ".js") == 0)
    {
      return "text/javascript";
    }
    else if(strcmp(dot, ".json" ) == 0)
    {
      return "application/json";
    }
    else if (strcmp(dot, ".mp3") == 0)
    {
        return "audio/mpeg";
    }
    return "text/plain";
}

int read_line(char* buffer,char* line)
{
  if(strcmp(buffer,"")==0) return 0; 
  int len = 0;
  char *ptr = strchr(buffer,'\n');
  if(ptr != NULL)
  {
    if(ptr == buffer)
    {
      strcpy(line,"\n");
      len = 1;
    }
    else
    {
      strncpy(line, buffer, ptr-buffer);
      len = strlen(line);
    }
  }
  return len;
}

int get_name_id(char *str, char *name, char *id)
{
    char *ptr1 = strchr(str,'?');
    if(ptr1 == NULL)
    {
      return 0;
    }
    ptr1 = ptr1 + 1;
    if(ptr1 == NULL)
    {
      return 0;
    }
    regex_t reg1;
    int reg = regcomp(&reg1, data_format3, REG_EXTENDED);
    if(reg)
    {
      regmatch_t pmatch[1];
      const size_t nmatch = 1;
      int status = regexec(&reg1, ptr1, nmatch, pmatch, 0);
      regfree(&reg1);
      if(status != 0)
      {
        return 1;
      }
    }
    ptr1 = strstr(str,"id="); //用strstr查找子串的首次位置
    char *ptr2 = strchr(str,'&');
    char *ptr3 = strstr(str,"name=");
    if(ptr1 != NULL && ptr2!=NULL && ptr3 != NULL)
    {
      if(ptr3 = ptr2 + 1)
      {
        strncpy(id,ptr1+3,ptr2-ptr1-3);
        strcpy(name,ptr3+5);
      }
    }
    else if(ptr1 != NULL) //只有id
    {
      strcpy(id,ptr1+3);
      name = NULL;
    }
    else if(ptr3 != NULL) //只有name
    {
      strcpy(name,ptr3+5);
      id = NULL;
    }
    return 1; 
}

int match_name_id(char* name, char* id, char* res)
{
  if(strcmp(id,"")==0 && strcmp(name,"")==0) return 0;
  char buf[2048]={'\0'};
  char file[15]={'\0'};
  strcpy(file,"data/data.json");
  int index=0,flag=0;
  pthread_mutex_lock(&file_lock);
  for(index=0;index<15;index++)
  {
    if(strcmp(file,file_list[index].name)==0)
    {
      strcpy(buf,file_list[index].content);
      flag = 1;
    }
    if(strcmp(file_list[index].name,"-1")==0)
    {
      break;
    }
  }
  pthread_mutex_unlock(&file_lock);
  if(!flag)
  {
    struct stat st;
    int ret = stat(file, &st);
    if(ret == -1) {
      printf("file not exist!\n");
      return 0;
    }
    int fd = open(file,O_RDONLY);
    if(fd == -1)
    {
      printf("open file failed!\n");
      return 0;
    }
    int len = read(fd, buf, 2048);
    if(len == -1)
    {
      return 0;
    }
    pthread_mutex_lock(&file_lock);
    strcpy(file_list[index].name,file);
    strcpy(file_list[index].content,buf);
    pthread_mutex_unlock(&file_lock);
    close(fd);
  }
  if(strcmp(id,"")==0)
  {
    char *ptr,*ptrl,*ptrr;
    ptrl = buf;
    int flag = 0;
    char temp[1024]={'\0'};
    while(1)
    {
      ptrr = strchr(ptrl,'}');
      if(ptrr == NULL) break;
      strncpy(temp,ptrl+1,ptrr-ptrl);
      ptr = strstr(temp,name);
      if(ptr != NULL)
      {
        if(flag) 
        {
          strcat(res,",");
        }
        else
        {
          strcat(res,"[");
        }
        flag = 1;
        strcat(res,temp);
      }
      ptrl = ptrr + 1;
    }
    if(flag)
    {
      strcat(res,"]");
      return 1;
    }
    else
    {
      return 0;
    }
  }
  else
  {
    char *ptr1 = strstr(buf,id);
    if(ptr1 == NULL)
    {
      return 0;
    }
    if(strcmp(name,"")==0)
    {
      char *ptr2 = strchr(ptr1,'}');
      char temp[1024]={'\0'};
      strncpy(temp,ptr1,ptr2-ptr1);
      strcat(res,"[{\"id\":");
      strcat(res,temp);
      strcat(res,"}]");
      return 1;
    }
    else
    {
      char *ptr2 = strstr(buf,name);
      char *ptr3 = strchr(ptr1,':');
      if(ptr2 == ptr3 + 2)
      {
        strcat(res,"[{\"id\":");
        strcat(res,id);
        strcat(res,",\"name\":\"");
        strcat(res,name);
        strcat(res,"\"}]");
        return 1;
      }
      else
      {
        return 0;
      }
    }
  }   
  return 1;
}

int data_format_match(char* text, char* content_type)
{
  // printf("\nhere2::%s\n",content_type);
  // int ret=strcmp(content_type,"application/x-www-form-urlencoded");
  // printf("%d\n",ret);
  if(strcmp(content_type,"application/x-www-form-urlencoded") == 0)
  {
    regex_t reg1;
    int reg = regcomp(&reg1, data_format1, REG_EXTENDED);
    if(!reg)
    {
      // printf("\n%s\n",text);
      char* ptr1 = strstr(text,"id=");
      char* ptr2 = strstr(text,"&name=");
      if(ptr1 != NULL && ptr2 != NULL && ptr1 < ptr2)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      regmatch_t pmatch[1];
      const size_t nmatch = 1;
      int status = regexec(&reg1, text, nmatch, pmatch, 0);
      regfree(&reg1);
      if(status == 0)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
  }
  else if(strcmp(content_type,"application/json") == 0)
  {
    regex_t reg2;
    int reg = regcomp(&reg2, data_format2, REG_EXTENDED);
    if(!reg)
    {
      char* ptr1 = strstr(text,"{\"id\":");
      char* ptr2 = strstr(text,",\"name\":");
      char* ptr3 = strstr(text,"\"}");
      if(ptr1 != NULL && ptr2 != NULL && ptr3 != NULL && ptr1 < ptr2 && ptr2 < ptr3)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      regmatch_t pmatch[1];
      const size_t nmatch = 1;
      int status = regexec(&reg2, text, nmatch, pmatch, 0);
      regfree(&reg2);
      if(status == 0)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
  }
  else
  {
    return 0;
  }
}

void send_response_head(int cfd, int status, char *title, char *type, char* protocol, int len)
{
  char buf[1024] = {'\0'};
  sprintf(buf, "%s %d %s\r\n", protocol, status, title);
  sprintf(buf+strlen(buf), "Content-Type:%s\r\n", type);
  sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len); 
  send(cfd, buf, strlen(buf), 0);
  send(cfd, "\r\n", 2, 0);
}

void response_with_html(int cfd, int status, char *title, char *text, char* protocol, int flag)
{
  char buf[1024] = {0};

  // html
  sprintf(buf, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
  sprintf(buf+strlen(buf),"   <meta charset=\"UTF-8\">\n");
  sprintf(buf+strlen(buf),"   <link rel=\"stylesheet\" href=\"/styles.css\"/>\n");
  sprintf(buf+strlen(buf),"<title>%d %s</title>\n</head>\n", status, title);
  sprintf(buf+strlen(buf), "<body>\n    <h1>\n        %s\n    </h1>\n",text);
  sprintf(buf+strlen(buf), "<hr>\n</body>\n</html>\n");
  
  // 头部
  if(flag)
  {
    send_response_head(cfd, status, title, "text/html", protocol, strlen(buf));
  }
  
  send(cfd, buf, strlen(buf), 0);
}

int send_response_file(int cfd, char* file)
{
  char buf[2048] = {'\0'};
  int index=0,flag=0,len=0;
  pthread_mutex_lock(&file_lock);
  for(index=0;index<15;index++)
  {
    if(strcmp(file,file_list[index].name)==0)
    {
      strcpy(buf,file_list[index].content);
      len = strlen(buf);
      flag = 1;
    }
    if(strcmp(file_list[index].name,"-1")==0)
    {
      break;
    }
  }
  pthread_mutex_unlock(&file_lock);
  if(!flag)
  {
    int fd = open(file,O_RDONLY);
    if(fd == -1)
    {
      printf("open file failed!\n");
      return 0;
    }
    len = read(fd, buf, 2048);
    if(len == -1)
    {
      return 0;
    }
    pthread_mutex_lock(&file_lock);
    strcpy(file_list[index].name,file);
    strcpy(file_list[index].content,buf);
    pthread_mutex_unlock(&file_lock);
    close(fd);
  }
  int ret = send(cfd, buf, len, 0);
  // printf("\n%d,%d\n",len,ret);
  if(ret == -1) {
    if(errno == EAGAIN){
      perror("send error:");
    } 
    else if(errno == EINTR){
      perror("send error:");
    } 
    else{
      perror("send error:");
      exit(1);
    }
  }
  return 1;
}

void notfound(int cfd, char* protocol)
{
  char file[1024]={'\0'};
  strcpy(file,"static/404.html");
  struct stat st;
  int ret = stat(file, &st);
  if(ret == -1) { 
    response_with_html(cfd, 404, "Not Found", "404 Not Found", protocol, 1);    
    return;
  }
  if(S_ISREG(st.st_mode)) { // 判断是否是一个文件        
    // 发送消息报头
    send_response_head(cfd, 404, "Not Found", get_file_type(file), protocol, st.st_size);
    // 发送文件内容
    if(!send_response_file(cfd, file))
    {
      response_with_html(cfd, 404, "Not Found", "404 Not Found", protocol, 0); 
    }
  }
}

void response_with_file(int cfd, int status, char* title, char* file, char* protocol)
{
  struct stat st;
  int ret = stat(file, &st);
  if(ret == -1) { 
    notfound(cfd,protocol);    
    return;
  }

  if(S_ISREG(st.st_mode)) { // 判断是否是一个文件        
    // 发送消息报头
    send_response_head(cfd, status, title, get_file_type(file), protocol, st.st_size);
    // 发送文件内容
    if(!send_response_file(cfd, file))
    {
      notfound(cfd,protocol);
    }
  }
}

void get_method(int cfd, char* path, char* protocol, int flag, char* name, char* id)
{
  if(flag)
  {
    if(strncasecmp("/api/search",path,11)==0)
    {
      char res[1024]={'\0'};
      if(match_name_id(name,id,res))  //match
      {
        send_response_head(cfd, 200, "OK", "application/json", protocol, strlen(res));
        send(cfd, res, strlen(res), 0);
      }
      else //not match 
      {
        strcpy(res,"data/not_found.json");
        response_with_file(cfd,404,"Not Found",res,protocol);
      }
    }
    else //error path
    {
      notfound(cfd,protocol);
    }
  }
  else
  {
    char* file = path+1; 
    if(strlen(file)==0){
      // printf("path empty\n");
      file="index.html";
    }

    int status=200;
    char title[20]={'\0'};
    strcpy(title,"OK");
    char dir_file[1024]={'\0'};
    //区分文件请求和检索请求
    if(strrchr(file, '.') != NULL)
    {
      if(strcmp(file,"403.html") == 0)
      {
        status=403;
        strcpy(title,"Forbidden");
      }
      else if(strcmp(file,"404.html") == 0)
      {
        status=404;
        strcpy(title,"Not Found");
      }
      else if(strcmp(file,"501.html") == 0)
      {
        status=501;
        strcpy(title,"Not Implemented");
      }
      else if(strcmp(file,"502.html") == 0)
      {
        status=502;
        strcpy(title,"Bad Gateway");
      }
      //所有文件请求都从static目录下获取
      strcpy(dir_file,"static/");
      strcat(dir_file,file);
    }
    else
    {
      strcpy(dir_file,"data/");
      if(strcmp(file, "api/check") == 0)
      {
        strcat(dir_file,"data.txt");
      }
      else if(strcmp(file, "api/list") == 0)
      {
        strcat(dir_file,"data.json");
      }
      else
      {
        notfound(cfd,protocol);
        return;
      }
    }
    file = dir_file;
    response_with_file(cfd,status,title,file,protocol);
  }
}

void post_method(int cfd, char* path, char* text, char* content_type, char* protocol)
{
  if(strcmp(path,"/api/echo") == 0)
  {
    int flag = data_format_match(text,content_type);
    if(flag)
    {
      send_response_head(cfd, 200, "OK", content_type, protocol, strlen(text));
      send(cfd, text, strlen(text), 0);
    }
    else
    {
      char file[16]={'\0'};
      strcpy(file,"data/error.txt");
      response_with_file(cfd,404,"Not Found",file,protocol);
    }
  }
  else if(strcmp(path,"/api/upload") == 0)
  {
    int flag = data_format_match(text,content_type);
    if(flag)
    {
      send_response_head(cfd, 200, "OK", content_type, protocol, strlen(text));
      send(cfd, text, strlen(text), 0);
    }
    else
    {
      char file[16]={'\0'};
      strcpy(file,"data/error.json");
      response_with_file(cfd,404,"Not Found",file,protocol);
    }
  }
  else
  {
    notfound(cfd,protocol);
  }
}

void notimplemented(int cfd, char* protocol)
{
  char path[1024]={'\0'};
  strcpy(path,"/501.html");
  get_method(cfd,path,protocol,0,NULL,NULL);
}

void disconnect(int cfd, int epfd)
{
    // 从epoll的树上摘下来
    // printf("\n%d,%d\n",cfd,epfd);
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if(ret == -1) {
        perror("epoll_ctl del cfd error");
        exit(1);
    }
    close(cfd);
}

void *http_handler(void *args)
{
  ConnectInfo* para = (ConnectInfo*) args;
  int cfd = para->cfd;
  int epfd = para->epfd;
  struct timeval timeout = {0, 1000};
  int ret=setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  char temp[1024] = {'\0'};
  char buffer[10240] = {'\0'};
  int len = recv(cfd,temp,1024,0);
  strcat(buffer,temp);
  while(len == 1024)
  {
    len = recv(cfd,temp,1024,0);
    strcat(buffer,temp);
  }
  // printf("\nbegin:\n%s\nend\n",buffer);
  char line[1024]={'\0'};
  len = read_line(buffer,line);
  int bias = len + 1;
  if(len > 0)
  {
    // printf("%d\n%s\n",len,line);
    //http请求报文头部形如“GET /index.html HTTP/1.1”
    char method[12],path[1024],protocol[12];
    sscanf(line,"%s %s %s",method,path,protocol); //sscanf()从字符串读取格式化输入
    int message_len = 0;
    char content_type[50]={'\0'};
    while(1)  //获取报文长度并将报文头部信息全部读取
    {
      char buf[1024] = {'\0'};
      int buf_len = read_line(buffer+bias,buf);
      bias = bias + buf_len + 1;
      if(strncasecmp("Content-Length",buf,14)==0) //strncasecmp()比较字符串前n个字符
      {
        strcpy(buf,buf+16);
        message_len = atoi(buf);
      }
      if(strncasecmp("Content-Type",buf,12)==0)
      {
        strcpy(content_type,buf+14);
        // printf("\nhere::%s\n",content_type);
      }
      if(strncasecmp("GET",buf,3)==0)
      {
        char name[100]={'\0'},id[100]={'\0'};
        int flag = get_name_id(path,name,id);
        get_method(cfd,path,protocol,flag,name,id);
        // printf("\n>%s\n>%s\n>%s\n",buffer+bias,buf,path);
        sscanf(buf,"%s %s %s",method,path,protocol);
      }
      if(buf[0] == '\n' || buf_len == 0)
      {
        break;
      }
    }
    //GET方法
    if(strncasecmp("GET",line,3)==0)
    {
      char name[100]={'\0'},id[100]={'\0'};
      int flag = get_name_id(path,name,id);
      get_method(cfd,path,protocol,flag,name,id);
    }
    else if(strncasecmp("POST",line,4)==0)  //POST方法
    {
      char* tmp = strrchr(content_type,'\r');//去掉content_type末尾的回车符
      *tmp = '\0';
      post_method(cfd,path,buffer+bias-1,content_type,protocol);
    }
    else
    {
      notimplemented(cfd,protocol);
    }
  }
  // close(cfd);
  disconnect(cfd,epfd);
}

void* client_send(void* args)
{
  cfdInfo* para = (cfdInfo*) args;
  char *buffer = malloc(10240*sizeof(char));
  struct timeval timeout = {3,0};
  int ret=setsockopt(para->cfd_down, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  while(1)
  {
    int len = recv(para->cfd_down,buffer,10240,0);
    if(len > 0)
    {
      // printf("%s",buffer);
      send(para->cfd_up,buffer,len,0);
    }
    else
    {
      break;
    }
  }
  free(buffer);
}

void* client_recv(void* args)
{
  cfdInfo* para = (cfdInfo*) args;
  char *buffer = malloc(20480*sizeof(char));
  struct timeval timeout = {3,0};
  int ret=setsockopt(para->cfd_up, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  int flag = 0;
  while(1)
  {
    int len = recv(para->cfd_up,buffer,20480,0);
    if(len > 0)
    {
      flag = 1;
      int len1 = send(para->cfd_down,buffer,len,0);
      // printf("\n%d,%d\n",len,len1);
    }
    else
    {
      if(!flag) 
      {
        response_with_html(para->cfd_down, 504, "Proxy Time-out", "504 Gateway/Proxy Time-out", "HTTP/1.1", 0);
      }
      break;
    }
  }
  free(buffer);
}

void* proxy_handler(void* args)
{
  ConnectInfo* para = (ConnectInfo*) args;
  char* psa = para->psa;
  int cfd = para->cfd;
  int epfd = para->epfd;
  char* ptr = strrchr(psa,':');
  char addr[40]={'\0'},pt[40]={'\0'};
  if(ptr != NULL)
  {
    strncpy(addr,psa,ptr-psa);
    strcpy(pt,ptr+1);
  }
  else
  {
    strcpy(addr,psa);
    strcpy(pt,"80");
  }
  
  // printf("\n%s\n%s\n",addr,pt);

  if(inet_addr(addr)==-1)
  {
    if(strlen(addr)<7) //未指定端口
    {
      strcpy(addr,psa);
      strcpy(pt,"80");
    }
    ptr = strchr(addr,'/');
    strcpy(addr,ptr+2);
    struct hostent *host = gethostbyname(addr);
    if(!host)
    {
      perror("host:");
      exit(1);
    }
    strcpy(addr,inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));
    printf("%s\n",addr);
  }

  struct sockaddr_in remote_addr;
  memset(&remote_addr,0,sizeof(remote_addr));
  remote_addr.sin_family=AF_INET;
  remote_addr.sin_addr.s_addr=inet_addr(addr);
  int port=atoi(pt);
  remote_addr.sin_port=htons(port);

  int sockfd;
  if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
  {  
    perror("socket");
  }

  if(connect(sockfd,(const struct sockaddr *)&remote_addr,sizeof(remote_addr))<0)
  {
    perror("connect");
    exit(1);
  }

  cfdInfo cinfo;
  cinfo.cfd_down=cfd;
  cinfo.cfd_up=sockfd;

  pthread_t th1,th2;
  // printf("begin\n");
  if(pthread_create(&th1, NULL, client_send, &cinfo)!=0)
  {
    perror("pthread_create failed");
    pthread_exit(NULL);
  }
  if(pthread_create(&th2, NULL, client_recv, &cinfo)!=0)
  {
    perror("pthread_create failed");
    pthread_exit(NULL);
  }
  pthread_join(th1, NULL);
  // printf("Finish1!\n");
  pthread_join(th2, NULL);
  // printf("Finish2!\n");
  // close(cfd);
  disconnect(cfd,epfd);
  close(sockfd);
}

int main(int argc, char *argv[])
{
  init();
  int port=8888,num_thread=2;
  char *ip;
  char *psa;
  if(argc < 7)
  {
    printf("Command invalid! Please check your command and try again!\n");
    printf("Example: ./httpserver --ip 127.0.0.1 --port 8888 --threads 8 [--proxy http://www.example.com:80]\n");
      return 1;
  }
  //解析参数
  for(int i=0; i<argc; i++)
  {
    if (strcmp("-i", argv[i]) == 0 || strcmp("--ip", argv[i]) == 0) 
        {
          ip = argv[++i];
          if(!ip) {
              fprintf(stderr, "expected argument after --ip\n");
          }
          else if(inet_addr(ip)==INADDR_NONE)
          {
              fprintf(stderr, "expected legal ip\n");
          }
        }
        else if(strcmp("-p",argv[i])==0 || strcmp("--port",argv[i])==0)  
        {
            char *port_str=argv[++i];
            if(!port_str){
                fprintf(stderr, "expected argument after --port\n");
            }
            port=atoi(port_str);
        }
        else if(strcmp("-t", argv[i]) == 0 || strcmp("--threads", argv[i]) == 0) 
        {
            char *threads = argv[++i];
            if(!threads) {
              fprintf(stderr, "expected argument after --proxy\n");
            }
            num_thread=atoi(threads);
        }
        else if(strcmp("--proxy", argv[i]) == 0)
        {
          psa = argv[++i];
          if(!psa) {
              fprintf(stderr, "expected argument after --proxy\n");
          }
        }
  }
  //创建套接字
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);   //s_addr按照网络字节顺序存储IP地址                        
  server_addr.sin_port = htons(port);  

    
  int server_sockfd;
  if((server_sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
  {  
    perror("socket");
    return 1;
  } 
  int temp = fcntl(server_sockfd, F_GETFL);
  temp |= O_NONBLOCK;
  fcntl(server_sockfd, F_SETFL, temp);
  int on = 1;     //设置端口重用
  if((setsockopt(server_sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)  
  {  
    perror("setsockopt failed");  
    return 1;  
  }   
  if (bind(server_sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr))<0) //绑定
  {
    perror("bind");
    return 1;
  }
  listen(server_sockfd, 2048); 

  // 创建一个epoll树的根节点
  int epfd = epoll_create(MAXSIZE);
  if(epfd == -1) {   
    perror("epoll_create error");
    exit(1);
  }

  // server_sockfd添加到epoll树上
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = server_sockfd;
  int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, server_sockfd, &ev);
  if(ret == -1) {  
      perror("epoll_ctl add lfd error");
      exit(1);
  }

  pool_init(num_thread);

  int count = 0;
  // 委托内核检测添加到树上的节点
  struct epoll_event event[MAXSIZE];
  while(1) {
        int ret = epoll_wait(epfd, event, MAXSIZE, 0);
        if(ret == -1) {
            perror("epoll_wait error");
            exit(1);
        }
        // 遍历发生变化的节点 epoll相对与select的优势，不用全部遍历
        for(int i=0; i < ret; i++)
        {
            // printf("\n%d,%d\n",i,ret);
            uint32_t events = event[i].events;
            if(events & EPOLLERR || events & EPOLLHUP || (! events & EPOLLIN)) {
                close (event[i].data.fd);
                continue;
            }
            if(event[i].data.fd == server_sockfd){
                // 接受连接请求
                struct sockaddr_in client_addr;
                int sin_size=sizeof(struct sockaddr_in);
                int cfd = accept(server_sockfd,(struct sockaddr *)&client_addr,&sin_size);
                if(cfd<0)
                {
                  perror("Error: accept");
                  return 1;
                }

                // 设置cfd为非阻塞
                int temp = fcntl(cfd, F_GETFL);
                temp |= O_NONBLOCK;
                fcntl(cfd, F_SETFL, temp);

                // 得到的新节点挂到epoll树上
                struct epoll_event ev;
                ev.data.fd = cfd;
                // 边沿非阻塞模式
                ev.events = EPOLLIN | EPOLLET;
                int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
                if(ret == -1) { 
                    perror("epoll_ctl add cfd error");
                    exit(1);
                }
                count++;
            } 
            else if(count>0)
            {
                ConnectInfo *cinfo = malloc(sizeof(ConnectInfo));
                if(psa != NULL)
                {
                  printf("\n%s\n",psa);
                  cinfo->psa = psa;
                  cinfo->cfd = event[i].data.fd;
                  cinfo->epfd = epfd;
                  pool_add_worker(proxy_handler,(void *)cinfo);
                }
                else
                {
                  cinfo->psa = psa;
                  cinfo->cfd = event[i].data.fd;
                  cinfo->epfd = epfd;
                  pool_add_worker(http_handler, (void *)cinfo);
                }
                count--;
            }
        }
  }
  pool_destroy();
  close(epfd);
  program_end();
  return 0;
}