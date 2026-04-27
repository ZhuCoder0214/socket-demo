#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define PORT 8888
#define BUF_MAX 1024
int main(){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in addr={0};
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(PORT);
    bind(fd,(struct sockaddr*)&addr,sizeof(addr));
    listen(fd,1);
    printf("waitting...\n");
    int cfd=accept(fd,NULL,NULL);
    //protobuf
    int len;
    recv(cfd,&len,sizeof(int),0);
    char buf[BUF_MAX]={0};
    recv(cfd,buf,len,0);
    printf("right:%s\n",buf);
    close(cfd);
    close(fd);
    return 0;
}
