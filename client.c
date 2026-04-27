#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SERVER_IP "192.168.56.103"
#define PORT 8888
int main(){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in addr={0};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(PORT);
    inet_pton(AF_INET,SERVER_IP,&addr.sin_addr);
    connect(fd,(struct sockaddr*)&addr,sizeof(addr));
    //protobuf
    char msg[]="helloworld";
    int len=strlen(msg);
    send(fd,&len,sizeof(int),0);
    send(fd,msg,len,0);
    printf("right:%s\n",msg);
    close(fd);
    return 0;
}
