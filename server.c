#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hello.pb-c.h"
#define PORT 8888
int main(){
    int listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr={0};
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(PORT);
    if(bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        perror("bind failed");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    if(listen(listen_fd,5)<0){
        perror("listen failed");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    printf("waiting client...\n");
    int client_fd=accept(listen_fd,NULL,NULL);
    if(client_fd<0){
        perror("accept failed");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    printf("client connected\n");
    uint32_t net_len;
    ssize_t ret=recv(client_fd,&net_len,sizeof(net_len),0);
    if(ret<=0){
        perror("recv len failed");
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    size_t msg_len=ntohl(net_len);
    uint8_t *buf=malloc(msg_len);
    if(!buf){
        perror("malloc failed");
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    ret=recv(client_fd,buf,msg_len,0);
    if(ret<=0){
        perror("recv msg failed");
        free(buf);
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    HelloWorldMsg *msg=hello_world_msg__unpack(NULL,msg_len,buf);
    if(!msg){
        printf("unpack failed\n");
        free(buf);
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    printf("recv: %s\n",msg->content);
    char reply[]="server received";
    ret=send(client_fd,reply,strlen(reply),0);
    if(ret<=0){
        perror("send reply failed");
    }
    hello_world_msg__free_unpacked(msg,NULL);
    free(buf);
    close(client_fd);
    close(listen_fd);
    return 0;
}
