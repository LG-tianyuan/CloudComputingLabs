/*
 * @Author: LG.tianyuan\zxy
 * @Date: 2022-05-26
 * @Update: 2022-06-02
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
#include "threadpool.h"

#define MAXSIZE 2000

static const char *search_format="^/api/search/(course[\?]id=[0-9a-zA-Z]+)|(course[\?]all)|(student[\?]id=[0-9]+)$";
static const char *post_data_format="^\\{\"student_id\":[\"]?[0-9]+[\"]?,\"course_id\":[\"]?[0-9a-zA-Z]+[\"]?\\}$";

char* file;

typedef struct {
    int num;  //集群主机个数
    int cfd[5];  //用于连接各主机的套接字
    int is_alive[5]; //记录连接是否断开
    int port[5];    //对应连接每个主机套接字端口
    char* ip[5];    //每个主机的ip
} ClusterInfo;

typedef struct {
    int cfd;
    int epfd;
    ClusterInfo cluster1;
    ClusterInfo cluster2;
} ConnectInfo;

void Connect_to_Server(ClusterInfo* cluster, int i) {
    struct sockaddr_in remote_addr;
    bzero(&remote_addr, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(cluster->ip[i]);
    remote_addr.sin_port = htons(cluster->port[i]);

    int client_sockfd;
    if ((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
    }
    if (connect(client_sockfd, (const struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0)
    {
        printf("Connect to %s:%d failed!\n", cluster->ip[i], cluster->port[i]);
        cluster->is_alive[i] = 0;
    }
    else {
        cluster->cfd[i] = client_sockfd;
        printf("Connect to %s:%d Successfully!\n", cluster->ip[i], cluster->port[i]);
        cluster->is_alive[i] = 1;
    }
}

void load_conf(char* file, ConnectInfo* cinfo) {
    FILE* fp = fopen(file, "r");
    if (!fp) {
        printf("can not open file\n");
        exit(0);
    }
    char line[1000] = { '\0' };
    int cnt = 0;
    int i = 0;
    while (!feof(fp)) {
        fgets(line, 1000, fp);
        if (line[0] != '!') {
            if (line[0] == 's') {
                i = 0;
                cnt++;
            }
            char temp1[30] = { '\0' }, temp2[30] = { '\0' };
            char* ptr1 = strchr(line, ' ');
            char* ptr2 = strrchr(line, ' ');
            strcpy(line, ptr1 + 1);
            char* ptr3 = strchr(line, ':');
            strncpy(temp1, line, ptr3 - line);
            strcpy(temp2, ptr2 + 1);
            int len = strlen(temp2);
            if (temp2[len - 1] == '\n') {
                temp2[len - 1] = '\0';
            }
            if (cnt == 1) {
                strcpy(cinfo->cluster1.ip[i], temp1);
                cinfo->cluster1.port[i] = atoi(temp2);
                cinfo->cluster1.num++;
            }
            else {
                strcpy(cinfo->cluster2.ip[i], temp1);
                cinfo->cluster2.port[i] = atoi(temp2);
                cinfo->cluster2.num++;
            }
            i++;
        }
    }
    fclose(fp);
}

int read_line(char* buffer, char* line)
{
    if (strcmp(buffer, "") == 0) return 0;
    int len = 0;
    char* ptr = strchr(buffer, '\n');
    if (ptr != NULL)
    {
        if (ptr == buffer)
        {
            strcpy(line, "\n");
            len = 1;
        }
        else
        {
            strncpy(line, buffer, ptr - buffer);
            len = strlen(line);
        }
    }
    return len;
}


int get_search_info(char* str, char* object, char* type, char* id)
{
    char ebuff[256];
    regex_t reg1;
    int reg = regcomp(&reg1, search_format, REG_EXTENDED);
    if (reg==0)
    {
        regerror(reg, search_format, ebuff, 256);
        //fprintf(stderr, "%s\n", ebuff);
        regmatch_t pmatch[1];
        const size_t nmatch = 1;
        int status = regexec(&reg1, str, nmatch, pmatch, 0);
        regfree(&reg1);
        if (status != 0)
        {
            return 0;
        }
    }
    char* ptr1 = strrchr(str, '/');
    char* ptr2 = strchr(str, '?');
    strncpy(object, ptr1 + 1, ptr2 - ptr1 - 1);
    if (ptr2[1] == 'i') {
        strcpy(type, "id");
        strcpy(id, ptr2 + 4);
    }
    else {
        strcpy(type, ptr2 + 1);
    }
    return 1;
}

int data_format_match(char* text)
{
    regex_t reg1;
    char ebuff[256];
   // printf("reg: %s\n", post_data_format);
    int reg = regcomp(&reg1, post_data_format, REG_EXTENDED);
    if (reg)
    {
        char* ptr1 = strstr(text, "{\"student_id\":");
        char* ptr2 = strstr(text, ",\"course_id\":");
        char* ptr3 = strstr(text, "}");
       // printf("ptr1:%s ptr2:%s ptr3:%s\n", ptr1, ptr2, ptr3);
        if (ptr1 != NULL && ptr2 != NULL && ptr3 != NULL && ptr1 < ptr2 && ptr2 < ptr3)
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
       // printf("%s\n", text);
        if (status == 0)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
}

void get_sid_cid(char* text, char* sid, char* cid)
{
    char* ptr1 = strchr(text, ':');
    char* ptr2 = strchr(text, ',');
    if (ptr1[1] == '\"') {
        strncpy(sid, ptr1 + 2, ptr2 - ptr1 - 2);
    }
    else {
        strncpy(sid, ptr1 + 1, ptr2 - ptr1 - 1);
    }
    int len = strlen(sid);
    if (sid[len - 1] == '\"') {
        sid[len - 1] = '\0';
    }
    ptr1 = strrchr(text, ':');
    ptr2 = strrchr(text, '}');
    if (ptr1[1] == '\"') {
        strncpy(cid, ptr1 + 2, ptr2 - ptr1 - 2);
    }
    else {
        strncpy(cid, ptr1 + 1, ptr2 - ptr1 - 1);
    }
    len = strlen(cid);
    if (cid[len - 1] == '\"') {
        cid[len - 1] = '\0';
    }
}

void send_response_head(int cfd, int status, char* title, char* type, char* protocol, int len)
{
    char buf[1024] = { '\0' };
    sprintf(buf, "%s %d %s\r\n", protocol, status, title);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);
}

void send_error(int cfd, char* protocol, char* msg)
{
    char buff[1024] = { '\0' };
    sprintf(buff, "{\"status\":\"error\",\"message\":\"");
    sprintf(buff + strlen(buff), "%s", msg);
    sprintf(buff + strlen(buff), "\"}\r\n");
    char title[30] = { '\0' }, type[30] = { '\0' };
    strcpy(title, "Forbidden");
    strcpy(type, "application/json");
    send_response_head(cfd, 403, title, type, protocol, strlen(buff));
    send(cfd, buff, strlen(buff), 0);
    send(cfd, "\r\n", 2, 0);
}

void send_success_message(int cfd, char* protocol)
{
    char buff[50] = { '\0' };
    sprintf(buff, "{\"status\":\"ok\"}\r\n");
    char title[30] = { '\0' }, type[30] = { '\0' };
    strcpy(title, "OK");
    strcpy(type, "application/json");
    send_response_head(cfd, 200, title, type, protocol, strlen(buff));
    send(cfd, buff, strlen(buff), 0);
    send(cfd, "\r\n", 2, 0);
}

void send_response_data(int cfd, char* protocol, char* msg)
{
    char buff[1024] = { '\0' };
    sprintf(buff, "{\"status\":\"ok\",\"data\":\"");
    sprintf(buff + strlen(buff), "%s", msg);
    sprintf(buff + strlen(buff), "\"}\r\n");
    char title[30] = { '\0' }, type[30] = { '\0' };
    strcpy(title, "OK");
    strcpy(type, "application/json");
    send_response_head(cfd, 200, title, type, protocol, strlen(buff));
    send(cfd, buff, strlen(buff), 0);
    send(cfd, "\r\n", 2, 0);
}

void send_and_recv(ClusterInfo* cluster, int cfd, char* text, int type, char* protocol) {
    char* buff = (char*)malloc(25600 * sizeof(char));
    char* result = (char*)malloc(25600 * sizeof(char));
    int is_send = 0;
    for (int i = 0; i < cluster->num; i++) {
        if (!cluster->is_alive[i]) {//如果已断开，尝试重连
            Connect_to_Server(cluster, i);
        }
        if (cluster->is_alive[i]) { //连接未断开才向该主机发信息
            int isclosed = send(cluster->cfd[i], text, strlen(text), 0);
            if (isclosed == -1) {
                cluster->is_alive[i] = 0;
                continue;
            }
            struct timeval timeout = { 2,0 }; //定时2秒
            setsockopt(cluster->cfd[i], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            char temp[1050] = { '\0' };
            int len = recv(cluster->cfd[i], temp, 1024, 0);
            if (type == 2 && len>0) {
               // printf("len0:%d temp0_len:%ld\n", len, strlen(temp));
                if (len==1024){
                    send(cfd, temp, strlen(temp), 0);
                    while (len == 1024)  //全部读取缓冲区内容
                    {
                        len = recv(cluster->cfd[i], temp, 1024, 0);
                       // printf("len:%d temp_len:%ld\n", len, strlen(temp));
                        send(cfd, temp, strlen(temp), 0);
                    }
                }
                else{
                    char line[200] = { "\0" };
                    len = read_line(temp, line);
                    len = read_line(temp+len+1, line);
                    if (line[len - 1] == '\n') {
                        line[len - 1] = '\0';
                    }
                    send_error(cfd, protocol, line);
                }
                is_send = 1;
                //free(temp);
            }
            if(len > 0 &&type!=2) {  //正常接收到回复
                strcpy(buff, temp);
                while (len == 1024)  //全部读取缓冲区内容
                {
                    len = recv(cluster->cfd[i], temp, 1024, 0);
                    strcat(buff, temp);
                }
                char line[1024] = { '\0' };
                char cid[50] = { '\0' }, cname[50] = { '\0' }, capacity[20] = { '\0' }, selected[20] = { '\0' };
                len = read_line(buff, line);
                int bias = len + 1;
                if (type == 1) { //GET Course id
                    if (strncasecmp("200", line, 3) == 0) {
                        len = read_line(buff + bias, line);
                        sscanf(line, "%s %s %s %s\n", cid, cname, capacity, selected);
                        strcpy(result, "{\"id\":");
                        strcat(result, cid);
                        strcat(result, ",\"name\":");
                        strcat(result, cname);
                        strcat(result, ",\"capacity\":");
                        strcat(result, capacity);
                        strcat(result, ",\"selected\":");
                        strcat(result, selected);
                        strcat(result, "}");
                        send_response_data(cfd, protocol, result);
                    }
                    else if (strncasecmp("403", line, 3) == 0) {
                        len = read_line(buff + bias, line);
                        printf("%s", line);
                        if (line[len - 1] == '\n') {
                            line[len - 1] = '\0';
                        }
                        send_error(cfd, protocol, line);
                    }
                }
                else if (type == 3) {  //GET Student id
                    if (strncasecmp("200", line, 3) == 0) {
                        char sid[50] = { '\0' }, sname[50] = { '\0' };
                        len = read_line(buff + bias, line);
                        bias = bias + len + 1;
                        sscanf(line, "%s %s\n", sid, sname);
                        //printf("cid:%s cid_size:%d cid==n:%d cid==0:%d cid==space:%d\n", cid, sizeof(cid), strcmp(cid, "\n"), strcmp(cid, "\0"),strcmp(cid,""));
                        strcpy(result, "{\"id\":");
                        strcat(result, sid);
                        strcat(result, ",\"name\":");
                        strcat(result, sname);
                        strcat(result, ",\"course\":[");

                        while (1) {
                            len = read_line(buff + bias, line);
                            //printf("line3:%s\n", line);
                            if (line[0] == '\n' || len == 0)
                            {
                                int res_len = strlen(result);
                                if(result[res_len - 1]==','){
                                  result[res_len - 1] = '\0';
                                }
                                break;
                            }
                            bias = bias + len + 1;
                            sscanf(line, "%s %s\n", cid, cname);
                            strcat(result, "{\"id\":");
                            strcat(result, cid);
                            strcat(result, ",\"name\":");
                            strcat(result, cname);
                            strcat(result, "},");
                        }
                        strcat(result, "]}");
                        //printf("STU_result:%s\n",result);
                        send_response_data(cfd, protocol, result);
                    }
                    else if (strncasecmp("403", line, 3) == 0) {
                        len = read_line(buff + bias, line);
                        if (line[len - 1] == '\n') {
                            line[len - 1] = '\0';
                        }
                        send_error(cfd, protocol, line);
                    }
                }
                else if (type == 4) {  //ADD or Drop
               // printf("line:%s\n ", line);
                    if (strncasecmp("200", line, 3) == 0) {
                        send_success_message(cfd, protocol);
                    }
                    else if (strncasecmp("403", line, 3) == 0) {
                        len = read_line(buff + bias, line);
                        if (line[len - 1] == '\n') {
                            line[len - 1] = '\0';
                        }
                        send_error(cfd, protocol, line);
                    }
                }
                is_send = 1;
            }
            else if (len == 0) { //连接断开
                cluster->is_alive[i] = 0;
            }//len为-1时，超时，表明当前主机非领导者
        }
        if (is_send) {
            break;
        }
    }
    free(buff);
    free(result);
}

void get_method(ConnectInfo* cinfo, int cfd, char* path, char* protocol, char* object, char* type, char* id)
{
    char buff[1024] = { '\0' };
    if (strcmp(object, "course") == 0 && strcmp(type, "id") == 0) {
        strcpy(buff, "GET Course ");
        strcat(buff, id);
        //send message to kvstore server
        send_and_recv(&cinfo->cluster1, cfd, buff, 1, protocol);
    }
    else if (strcmp(object, "course") == 0 && strcmp(type, "all") == 0) {
        strcpy(buff, "GET Course all");
        //send message to kvstore server
        send_and_recv(&cinfo->cluster2, cfd, buff, 2, protocol);
    }
    else if (strcmp(object, "student") == 0 && strcmp(type, "id") == 0) {
        strcpy(buff, "GET Student ");
        strcat(buff, id);
        //send message to kvstore server
        if (strcmp(id, "202208011050") > 0) {
            send_and_recv(&cinfo->cluster2, cfd, buff, 3, protocol);
        }
        else {
            send_and_recv(&cinfo->cluster1, cfd, buff, 3, protocol);
        }
    }
    else {
        char msg[30] = { '\0' };
        strcpy(msg, "invalid query string");
        send_error(cfd, protocol, msg);
    }
}

void post_method(ConnectInfo* cinfo, int cfd, char* path, char* text, char* protocol)
{
    char buff[1024] = { '\0' }, sid[20] = { '\0' }, cid[20] = { '\0' };
    if (strcmp(path, "/api/choose") == 0)
    {
        int flag = data_format_match(text);
        if (flag)
        {
            get_sid_cid(text, sid, cid);
            strcpy(buff, "ADD Student Course ");
            strcat(buff, sid);
            strcat(buff, " ");
            strcat(buff, cid);
            printf("choose buff:%s\n", buff);
            if (strcmp(sid, "202208011050") > 0) {
                send_and_recv(&cinfo->cluster2, cfd, buff, 4, protocol);
            }
            else {
                send_and_recv(&cinfo->cluster1, cfd, buff, 4, protocol);
            }
        }
        else
        {
            char msg[30] = { '\0' };
            strcpy(msg, "Data format error");
            send_error(cfd, protocol, msg);
        }
    }
    else if (strcmp(path, "/api/drop") == 0)
    {
        int flag = data_format_match(text);
        if (flag)
        {
            get_sid_cid(text, sid, cid);
            strcpy(buff, "DEL Student Course ");
            strcat(buff, sid);
            strcat(buff, " ");
            strcat(buff, cid);
            if (strcmp(sid, "202208011050") > 0) {
                send_and_recv(&cinfo->cluster2, cfd, buff, 4, protocol);
            }
            else {
                send_and_recv(&cinfo->cluster1, cfd, buff, 4, protocol);
            }
        }
        else
        {
            char msg[30] = { '\0' };
            strcpy(msg, "Data format error");
            send_error(cfd, protocol, msg);
        }
    }
    else
    {
        char msg[30] = { '\0' };
        strcpy(msg, "invalid query string");
        send_error(cfd, protocol, msg);
    }
}

void disconnect(int cfd, int epfd)
{
    // 从epoll的树上摘下来
    // printf("\n%d,%d\n",cfd,epfd);
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if (ret == -1) {
        perror("epoll_ctl del cfd error");
        exit(1);
    }
    close(cfd);
}

void* http_handler(void* args)
{
    ConnectInfo* para = (ConnectInfo*)args;
    int cfd = para->cfd;
    int epfd = para->epfd;

    for (int i = 0; i < 5; i++) {
        para->cluster1.ip[i] = malloc(20 * sizeof(char));
        para->cluster2.ip[i] = malloc(20 * sizeof(char));
    }

    load_conf(file, para);
    for (int i = 0; i < para->cluster1.num; i++) {
        Connect_to_Server(&para->cluster1, i);
    }
    for (int i = 0; i < para->cluster2.num; i++) {
        Connect_to_Server(&para->cluster2, i);
    }

    struct timeval timeout = { 0, 1000 };
    int ret = setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    char temp[1024] = { '\0' };
    char buffer[10240] = { '\0' };
    int len = recv(cfd, temp, 1024, 0);
    strcat(buffer, temp);
    while (len == 1024)
    {
        len = recv(cfd, temp, 1024, 0);
        strcat(buffer, temp);
    }
    char line[1024] = { '\0' };
    len = read_line(buffer, line);
    int bias = len + 1;
    if (len > 0)
    {
        //http请求报文头部形如“GET /index.html HTTP/1.1”
        char method[12], path[1024], protocol[12];
        sscanf(line, "%s %s %s", method, path, protocol); //sscanf()从字符串读取格式化输入
        int message_len = 0;
        char content_type[50] = { '\0' };
        while (1)  //获取报文长度并将报文头部信息全部读取
        {
            char buf[1024] = { '\0' };
            int buf_len = read_line(buffer + bias, buf);
            bias = bias + buf_len + 1;
            if (strncasecmp("Content-Length", buf, 14) == 0) //strncasecmp()比较字符串前n个字符
            {
                strcpy(buf, buf + 16);
                message_len = atoi(buf);
            }
            if (strncasecmp("Content-Type", buf, 12) == 0)
            {
                strcpy(content_type, buf + 14);
            }
            if (buf[0] == '\n' || buf_len == 0)
            {
                break;
            }
        }
        //GET方法
        if (strncasecmp("GET", line, 3) == 0)
        {
            char object[20] = { '\0' }, type[20] = { '\0' }, id[100] = { '\0' };
            int flag = get_search_info(path, object, type, id);
            if (flag) {
                get_method(para, cfd, path, protocol, object, type, id);
            }
            else {
                char msg[30] = { '\0' };
                strcpy(msg, "invalid query string");
                send_error(cfd, protocol, msg);
            }
        }
        else if (strncasecmp("POST", line, 4) == 0)  //POST方法
        {
            post_method(para, cfd, path, buffer + bias - 1, protocol);
        }
        else
        {
            char msg[30] = { '\0' };
            strcpy(msg, "invalid query string");
            send_error(cfd, protocol, msg);
        }
    }
    for (int i = 0; i < 5; i++) {
        free(para->cluster1.ip[i]);
        free(para->cluster2.ip[i]);
    }
    // close(cfd);
    disconnect(cfd, epfd);
}

int main(int argc, char* argv[])
{
    file = (char*)malloc(50);
    int port = 8888, num_thread = 2;
    char* ip;
    if (argc < 9)
    {
        printf("Command invalid! Please check your command and try again!\n");
        printf("Example: ./httpserver --ip 127.0.0.1 --port 8888 --threads 8 --config_path server.conf\n");
        return 1;
    }
    //解析参数
    for (int i = 0; i < argc; i++)
    {
        if (strcmp("-i", argv[i]) == 0 || strcmp("--ip", argv[i]) == 0)
        {
            ip = argv[++i];
            if (!ip) {
                fprintf(stderr, "expected argument after --ip\n");
            }
            else if (inet_addr(ip) == INADDR_NONE)
            {
                fprintf(stderr, "expected legal ip\n");
            }
        }
        else if (strcmp("-p", argv[i]) == 0 || strcmp("--port", argv[i]) == 0)
        {
            char* port_str = argv[++i];
            if (!port_str) {
                fprintf(stderr, "expected argument after --port\n");
            }
            port = atoi(port_str);
        }
        else if (strcmp("-t", argv[i]) == 0 || strcmp("--threads", argv[i]) == 0)
        {
            char* threads = argv[++i];
            if (!threads) {
                fprintf(stderr, "expected argument after --threads\n");
            }
            num_thread = atoi(threads);
        }
        else if (strcmp("-c", argv[i]) == 0 || strcmp("--config_path", argv[i]) == 0)
        {
            char* temp = argv[++i];
            if (!temp) {
                fprintf(stderr, "expected argument after --config_path\n");
            }
            else {
                strcpy(file, temp);
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
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }
    int temp = fcntl(server_sockfd, F_GETFL);
    temp |= O_NONBLOCK;
    fcntl(server_sockfd, F_SETFL, temp);
    int on = 1;     //设置端口重用
    if ((setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
    {
        perror("setsockopt failed");
        return 1;
    }
    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) //绑定
    {
        perror("bind");
        return 1;
    }
    listen(server_sockfd, 2048);

    // 创建一个epoll树的根节点
    int epfd = epoll_create(MAXSIZE);
    if (epfd == -1) {
        perror("epoll_create error");
        exit(1);
    }

    // server_sockfd添加到epoll树上
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_sockfd;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, server_sockfd, &ev);
    if (ret == -1) {
        perror("epoll_ctl add lfd error");
        exit(1);
    }

    pool_init(num_thread);

    int count = 0;
    // 委托内核检测添加到树上的节点
    struct epoll_event event[MAXSIZE];
    while (1) {
        int ret = epoll_wait(epfd, event, MAXSIZE, 0);
        if (ret == -1) {
            perror("epoll_wait error");
            exit(1);
        }
        // 遍历发生变化的节点 epoll相对与select的优势，不用全部遍历
        for (int i = 0; i < ret; i++)
        {
            // printf("\n%d,%d\n",i,ret);
            uint32_t events = event[i].events;
            if (events & EPOLLERR || events & EPOLLHUP || (!events & EPOLLIN)) {
                close(event[i].data.fd);
                continue;
            }
            if (event[i].data.fd == server_sockfd) {
                // 接受连接请求
                struct sockaddr_in client_addr;
                unsigned int sin_size = sizeof(struct sockaddr_in);
                int cfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &sin_size);
                if (cfd < 0)
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
                if (ret == -1) {
                    perror("epoll_ctl add cfd error");
                    exit(1);
                }
                count++;
            }
            else if (count > 0)
            {
                ConnectInfo* cinfo = (ConnectInfo*)malloc(sizeof(ConnectInfo));
                cinfo->cfd = event[i].data.fd;
                cinfo->epfd = epfd;
                pool_add_worker(http_handler, (void*)cinfo);
                count--;
            }
        }
    }
    pool_destroy();
    close(epfd);
    return 0;
}