/*
 *@Author:LG-tianyuan
 *@Date:2022-5-23
 *@Last-Update:2022-5-26
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include "resp.hxx"
#define _CLM_RED "\033[31m"
#define _CLM_GREEN "\033[32m"
#define _CLM_END "\033[0m"
#define _CLM_YELLOW "\033[33m"
using namespace std;

int cfd[10];
int port[10];
pthread_t th[10];
int num;
std::vector<std::string> ip;
std::string command;
bool is_alive[10];
bool is_recv;

void init(){
	for(int i=0;i<num;i++){
		port[i] = 9001+i;
        is_alive[i] = false;
	}
	is_recv = false;
}

void help()
{
    std::cout
    << _CLM_YELLOW
    << "Usage:\n"
    << "@ set value: SET key value\n"
    << "	e.g. SET CS06142 \"Cloud Computing\"\n"
    << "@ get value: GET key\n"
    << "	e.g. GET CS06142\n"
    << "@ delete value: DELETE key1 key2 ...\n"
    << "	e.g. DELETE CS06142\n"
    << "@ to quit: exit\n"
    << _CLM_END;
}

void Connect_to_Server(int i){
    struct sockaddr_in remote_addr;
    bzero(&remote_addr,sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(ip[i].c_str());
    remote_addr.sin_port = htons(port[i]);

    int client_sockfd;
    if((client_sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
        perror("socket");
    }
    if(connect(client_sockfd,(const struct sockaddr *)&remote_addr,sizeof(remote_addr))<0)
    {
        printf("Connect to %s:%d failed!\n",ip[i].c_str(),port[i]);
    }else{
        cfd[i] = client_sockfd;
        printf("Connect to %s:%d Successfully!\n",ip[i].c_str(),port[i]);
        is_alive[i] = true;
    }
}

void* client_recv(void* args){
	int* arg = (int*) args;
	int client_sockfd = *arg;
	char *buff = (char*)malloc(1024*sizeof(char));
	struct timeval timeout = {2,0};
  	setsockopt(client_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  	int len=0;
  	len = recv(client_sockfd,buff,1024,0);
  	if(len > 0){
        if(strcmp(buff,"-1")!=0){
            std::string msg(buff);
            if(command=="GET"){
                std::cout<<DecodeArrays(msg)<<std::endl;
            }else if(command=="SET"){
                std::cout<<DecodeSuccessMsg(msg)<<std::endl;
            }else{
                std::cout<<DecodeInteger(msg)<<std::endl;
            }
            is_recv = true;
        }
  	}
    free(buff);
}

void client_receive(int i,int client_sockfd){
    if(is_alive[i]){
        char buff[1024] = {'\0'};
        struct timeval timeout = {2,0};
        setsockopt(client_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        int len=0;
        len = recv(client_sockfd,buff,1024,0);
        // printf("%dlen:%d\n",i+1,len);
        if(len>0){
            if(strcmp(buff,"-1")!=0){
                std::string msg(buff);
                if(command=="GET"){
                    std::cout<<DecodeArrays(msg)<<std::endl;
                }else if(command=="SET"){
                    std::cout<<DecodeSuccessMsg(msg)<<std::endl;
                }else{
                    std::cout<<DecodeInteger(msg)<<std::endl;
                }
                is_recv = true;
            } 
        }else{
            is_alive[i]=false;
            if(len == -1){
                printf("Server: Time out or Serving other client! Try again!\n");
            }
        }
    }
}

void do_cmd(std::string s){
    for(int i=0;i<num;i++){
        if(!is_alive[i]){
            printf("Try to reconnect to %s:%d.\n",ip[i].c_str(),port[i]);
            Connect_to_Server(i);
        }
    }
	char buff[1024]={'\0'};
	strcpy(buff,s.c_str());
	for(int i=0;i<num;i++){
        if(is_alive[i]){
            int isclosed = send(cfd[i],buff,strlen(buff),0);
            // printf("%d:%d\n",i+1,isclosed);
            if(isclosed == -1){
                is_alive[i] = false;
            }
        }
	}
    if(command == "exit"){
        exit(0);
    }
  	for(int i=0;i<num;i++){
        client_receive(i,cfd[i]);
  	}
    if(!is_recv){
    	std::cout<<"ERROR"<<std::endl;
    }
}

//按空格拆分命令，tokenization
bool tokenize_and_check(const char* str, char c = ' ') {
    int cnt = 0;
    do {
        const char *begin = str;
        while(*str != c && *str) str++;
        if (begin != str) {
        	std::string temp = std::string(begin,str);
        	cnt++;
        	if(cnt==1){
        		command = temp;
        		if(command!="SET"&&command!="GET"&&command!="DELETE"&&command!="exit"){
        			return false;
        		}
        	}
        }
        if(cnt>=3) break;
    } while (0 != *str++);
    if(command!="exit"&&(cnt<2||(command=="SET"&&cnt==2))){
    	return false;
    }
    return true;
}

void loop(){
	char cmd[1024];
    std::string prompt = "client > ";
    while (true) {
#if defined(__linux__) || defined(__APPLE__)
        std::cout << _CLM_GREEN << prompt << _CLM_END;
#else
        std::cout << prompt;
#endif
        if (!std::cin.getline(cmd, 1000)) {
            break;
        }

        is_recv = false;
        bool flag = tokenize_and_check(cmd);
        if(!flag){
        	std::cout<<"Invalid command!\n";
        	help();
        }else{
        	std::string msg(cmd);
            if(command!="exit"){
                msg = EncodeArray(msg);
            }
        	do_cmd(msg);
        }
    }
}

void load_conf(char* file){
    FILE *fp = fopen(file,"r");
    if(!fp){
        printf("can not open file\n");
        exit(0);
    }
    char line[1000]={'\0'};
    while(!feof(fp)){
        fgets(line,1000,fp);
        if(line[0]!='!'){
            char* ptr = strchr(line,' ');
            strcpy(line,ptr+1);
            char temp[100]={'\0'};
            char* ptr1 = strchr(line,':');
            strncpy(temp,line,ptr1-line);
            int len = strlen(temp);
            if(temp[len-1]=='\n'){
                temp[len-1] = '\0';
            }
            ip.push_back(std::string(temp));
        }
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    num = 4;
	if (argv[1] != NULL)
    {
        num = atoi(argv[1])>4 ? atoi(argv[1]) : num;
    }
    init();
    if(argc == 3 && argv[2] != NULL)
    {
        load_conf(argv[2]);
    }
    else
    {
        for(int i=0;i<num;i++){
            ip.push_back("127.0.0.1");
        }
    }
    for(int i=0; i<num; i++){
        Connect_to_Server(i);
    }
    loop();
    return 0;
}
