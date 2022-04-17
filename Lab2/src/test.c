#include <stdio.h>
#include <string.h>
int read_line(char* buffer,char* line)
{
  printf("22\n");
  char *ptr = strchr(buffer,'\n');
  strncpy(line, buffer, ptr-buffer);
  strcpy(buffer,ptr+1);
  int len = strlen(line);
  return len;
}
int main()
{
	char text[100]={'\0'};
	strcpy(text,"GET /index.html HTTP/1.1\r\nContent-length:1234\r\nContent-Type:text/plain\r\n\r\n");
	// char method[12],path[16],protocol[12],content[20];
 //    sscanf(text,"%s %s %s %s",method,path,protocol,content); 
 //    printf("\n%s\n%s\n%s\n%s\n",method,path,protocol,content);
	char line[100]={'\0'};
	int len = read_line(text,line);
	printf("\n%s\n%s\n",text,line);
    return 0;
}