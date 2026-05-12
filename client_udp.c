#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#define SERVER_IP "192.168.56.103"
#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 2
#define WINDOW_SIZE 3
#define MAX_SEQ 20
int main(){
    char send_buf[BUFFER_SIZE];
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int base=0;
    int nextSeq=0;
    char user_input[BUFFER_SIZE];
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT);
    if(inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr)<=0){
        perror("inet_pton failed,please check ip");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    struct timeval tv;
    tv.tv_sec=TIMEOUT_SEC;
    tv.tv_usec=0;
    if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv))<0){
        perror("setsockopt failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
        printf("[CLIENT] window_size:%d\n",WINDOW_SIZE);
        printf("[CLIENT] line server %s:%d,data...\n",SERVER_IP,SERVER_PORT);
    while(1){
        printf("send...:");
        fgets(user_input,BUFFER_SIZE,stdin);
        user_input[strcspn(user_input,"\n")]=0;
        if(strcmp(user_input,"exit")==0){
            printf("[CLIENT]close...\n");
            break;
        }
        while(nextSeq<base+WINDOW_SIZE&&nextSeq<MAX_SEQ){
            snprintf(send_buf,sizeof(send_buf),"%d:%s",nextSeq,user_input);
            printf("[CLIENT] seq=%d",nextSeq);
            int len=strlen(send_buf);
            sendto(sockfd,send_buf,len,0,(struct sockaddr*)&server_addr,sizeof(server_addr));
            nextSeq++;
            sleep(1);
        }
        socklen_t addr_len=sizeof(server_addr);
        ssize_t n=recvfrom(sockfd,buffer,BUFFER_SIZE,0,(struct sockaddr*)&server_addr,&addr_len);
        if(n<0){
            if(errno==EWOULDBLOCK||errno==EAGAIN){
                printf("[CLIENT] overtime,again base=%d~%d\n",base,nextSeq-1);
                for(int i=base;i<nextSeq;i++){
                    snprintf(send_buf,sizeof(send_buf),"%d:%s",i,user_input);
                    sendto(sockfd,send_buf,strlen(send_buf),0,(struct sockaddr*)&server_adde,sizeof(server_addr));
                }
            }else{
                perror("recvfrom failed");
                break;
            }
        }else{
            buffer[n]='\0';
            if(strncmp(buffer,"ACK:",4)==0){
                int ack_seq=atoi(buffer+4);
                printf("[CLIENT] success ACK:%d\n",ack_seq);
                if(ack_seq>=base){
                    base=ack_seq+1;
                    peintf("[CLIENT] new base=%d\n",base);
                 }
            }
        }
        if(base>=MAX_SEQ){
            printf("[CLIENT] finish\n");
            break;
        }
    }
    close(sockfd);
    printf("[CLIENT] exit\n");
    return 0;
}
