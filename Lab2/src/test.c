#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
// int read_line(char* buffer,char* line)
// {
//   printf("22\n");
//   char *ptr = strchr(buffer,'\n');
//   strncpy(line, buffer, ptr-buffer);
//   strcpy(buffer,ptr+1);
//   int len = strlen(line);
//   return len;
// }
int main(int argc, char *argv[])
{
	char text[100]={'\0'};
	strcpy(text,"http://www.hnu.edu.cn");
  if (argv[1] != NULL)
  {
        strcpy(text,argv[1]);
  }
	// char method[12],path[16],protocol[12],content[20];
 //    sscanf(text,"%s %s %s %s",method,path,protocol,content); 
 //    printf("\n%s\n%s\n%s\n%s\n",method,path,protocol,content);
	// char line[100]={'\0'};
	// int len = read_line(text,line);
	// printf("\n%s\n%s\n",text,line);
  // int ret = inet_addr(text);
  // printf("%d\n",ret);
  char *ptr = strchr(text,'/');
  strcpy(text,ptr+2);
  struct hostent *host = gethostbyname(text);
  if(!host)
  {
    perror("host:");
    // exit(1);
  }
  for(int i=0; host->h_addr_list[i]; i++){
        printf("IP addr %d: %s\n", i+1, inet_ntoa( *(struct in_addr*)host->h_addr_list[i] ) );
  }
  //strcpy(text,inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));
  // printf("%s\n",text);
  return 0;
}