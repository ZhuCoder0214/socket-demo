#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include "hello.pb-c.h"

#define SERVER_IP "192.168.56.103"
#define PORT 8888

int main(){
    int sock_fd=socket(AF_INET,SOCK_STREAM,0);
    if(sock_fd<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr={0};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(PORT);
    inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr);
    
    if(connect(sock_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        perror("connect failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("connect success\n");
    
    HelloWorldMsg msg=HELLO_WORLD_MSG__INIT;
    msg.content="hello from client";
    
    size_t msg_len=hello_world_msg__get_packed_size(&msg);
    uint8_t *buf=malloc(msg_len);
    if(!buf){
        perror("malloc failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    hello_world_msg__pack(&msg,buf);
    
    uint32_t net_len=htonl(msg_len);
    ssize_t ret=send(sock_fd,&net_len,sizeof(net_len),0);
    if(ret<=0){
        perror("send len failed");
        free(buf);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    
    ret=send(sock_fd,buf,msg_len,0);
    if(ret<=0){
        perror("send msg failed");
        free(buf);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("send success\n");
    
    char recv_buf[64]={0};
    ret=recv(sock_fd,recv_buf,sizeof(recv_buf)-1,0);
    if(ret>0){
        printf("server reply:%s\n",recv_buf);
    }
    free(buf);
    close(sock_fd);
    return 0;
}
    
