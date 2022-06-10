/*
 * @Author: LG.tianyuan
 * @Date: 2022-06-02
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
#include <assert.h>
#include <sys/epoll.h>
#include <time.h>
#include "threadpool.h"

using namespace std;

typedef struct {
  int cfd_down;
  int cfd_up;
} cfdInfo;

typedef struct {
  int cfd;
  int epfd;
  int num;
  int cfd[3];
  int is_alive[3]; //记录连接是否断开
  int port[3];    //对应连接每个主机套接字端口
  char* ip[3];    //每个主机的ip
} ConnectInfo;

void Connect_to_Server(ConnectInfo* cinfo,int i){
    struct sockaddr_in remote_addr;
    bzero(&remote_addr,sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(cinfo->ip[i]);
    remote_addr.sin_port = htons(cinfo->port[i]);

    int client_sockfd;
    if((client_sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
        perror("socket");
    }
    if(connect(client_sockfd,(const struct sockaddr *)&remote_addr,sizeof(remote_addr))<0)
    {
        printf("Connect to %s:%d failed!\n",cinfo->ip[i],cinfo->port[i]);
        cinfo->is_alive[i] = 0;
    }else{
        cinfo->cfd[i] = client_sockfd;
        printf("Connect to %s:%d Successfully!\n",cinfo->ip[i],cinfo->port[i]);
        cinfo->is_alive[i] = 1;
    }
}

void load_conf(char* file, ConnectInfo* cinfo){
    FILE *fp = fopen(file,"r");
    if(!fp){
        printf("can not open file\n");
        exit(0);
    }
    cinfo->num = 0;
    char line[1000]={'\0'};
    int cnt = 0;
    int i = 0;
    while(!feof(fp)){
        fgets(line,1000,fp);
        if(line[0]!='!'){
        	cinfo->num++;
        	char* ptr = strchr(line,' ');
			strcpy(line,ptr+1);
			char temp[100]={'\0'};
            char* ptr1 = strrchr(line,':');
            strncpy(temp,line,ptr1-line);
            strcpy(cinfo->ip[i],temp);
            strcpy(temp,ptr1+1);
            cinfo->port[i] = atoi(temp);
            i++;
        }
    }
    fclose(fp);
}

void send_error(int cfd, char* protocol,char* msg)
{
  char buff[1024]={'\0'};
  sprintf(buff,"{\"status\":\"error\",\"message\":\"");
  sprintf(buff+strlen(buff),"%s",msg);
  sprintf(buff+strlen(buff),"\"}\r\n");
  char title[30]={'\0'},type[30]={'\0'};
  strcpy(title,"Forbidden");
  strcpy(type,"application/json");
  send_response_head(cfd,403,title,type,protocol,strlen(buff));
  send(cfd, buff, strlen(buff), 0);
  send(cfd, "\r\n", 2, 0);
}

void* client_send(void* args)
{
  cfdInfo* para = (cfdInfo*) args;
  char *buffer = malloc(1024*sizeof(char));
  struct timeval timeout = {3,0};
  int ret=setsockopt(para->cfd_down, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  while(1)
  {
    int len = recv(para->cfd_down,buffer,1024,0);
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
  char *buffer = malloc(25600*sizeof(char));
  struct timeval timeout = {3,0};
  int ret=setsockopt(para->cfd_up, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  int flag = 0;
  while(1)
  {
    int len = recv(para->cfd_up,buffer,25600,0);
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
        char err[50]={'\0'},protocol[20]={'\0'};
				strcpy(err,"Website temporarily unavailable");
				strcpy(protocol,"HTTP/1.1");
				send_error(cfd,protocol,err);
			}
      break;
    }
  }
  free(buffer);
}

void* proxy_handler(void* args)
{
  ConnectInfo* para = (ConnectInfo*) args;
  int cfd = para->cfd;
  int epfd = para->epfd;

  for(int i = 0; i < 3; i++){
    para->ip[i] = malloc(20*sizeof(char));
  }

  load_conf(file,para);

  bool flag = false;
  for(int i = 0 ; i < para->num; i++){
  	Connect_to_Server(para,i);
  	if(para->is_alive[i]){
  		flag = true;
  	}
  }

  if(flag){
  	cfdInfo cfdinfo;
	  cfdinfo.cfd_down=cfd;
	  srand((unsigned)time(NULL));
	  while(1){
	  	int no=rand()%para->num;
	  	if(is_alive[no]){
	  		cfdinfo.cfd_up = para->cfd[no];
	  	}
	  }

	  pthread_t th1,th2;
	  // printf("begin\n");
	  if(pthread_create(&th1, NULL, client_send, &cfdinfo)!=0)
	  {
	    perror("pthread_create failed");
	    pthread_exit(NULL);
	  }
	  if(pthread_create(&th2, NULL, client_recv, &cfdinfo)!=0)
	  {
	    perror("pthread_create failed");
	    pthread_exit(NULL);
	  }
	  pthread_join(th1, NULL);
	  pthread_join(th2, NULL);
  }else{
		char err[50]={'\0'},protocol[20]={'\0'};
		strcpy(err,"Website temporarily unavailable");
		strcpy(protocol,"HTTP/1.1");
		send_error(cfd,protocol,err);
  }
	  
  disconnect(cfd,epfd);
  close(sockfd);
  for(int i = 0; i < 3; i++){
    free(para->ip[i]);
  }
}

int main(int argc, char *argv[])
{
  file = (char*)malloc(50);
  int port=8888,num_thread=2;
  char *ip;
  if(argc < 9)
  {
    printf("Command invalid! Please check your command and try again!\n");
    printf("Example: ./httpserver --ip 127.0.0.1 --port 8888 --threads 8 --config httpserver.conf\n");
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
              fprintf(stderr, "expected argument after --threads\n");
            }
            num_thread=atoi(threads);
        }
        else if(strcmp("--config", argv[i]) == 0)
        {
          char* temp = argv[++i];
          if(!temp) {
              fprintf(stderr, "expected argument after --config\n");
          }else{
            strcpy(file,temp);
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
                unsigned int sin_size=sizeof(struct sockaddr_in);
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
            } 
            else
            {
                ConnectInfo *cinfo = (ConnectInfo*)malloc(sizeof(ConnectInfo));
                cinfo->cfd = event[i].data.fd;
                cinfo->epfd = epfd;
                pool_add_worker(proxy_handler, (void *)cinfo);
            }
        }
  }
  pool_destroy();
  close(epfd);
  return 0;
}