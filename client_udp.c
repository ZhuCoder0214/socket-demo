#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
//#define USE_GBN
#define USE_SR//通过宏切换两种协议 
#define SERVER_IP "192.168.56.103"
#define SERVER_PORT 12345
#define BUFFER_SIZE 2048
#define TIMEOUT_SEC 2
#define WINDOW_SIZE 3
#define MAX_SEQ 20
typedef struct{
    int seq;
    int acked;
    char data[BUFFER_SIZE];
    time_t send_time;
}Packet;
Packet send_window[WINDOW_SIZE];//发送窗口数组 
int base=0;
int next_seq=0;
int main(){
    char send_buf[BUFFER_SIZE];
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char user_input[BUFFER_SIZE];
    int dup_ack_count=0;//重复ACK计数器 
    int last_ack_seq=-1;
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    //配置服务器地址 
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT);
    if(inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr)<=0){
        perror("inet_pton failed,please check ip");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    //设置套接字超时 
    struct timeval tv;
    tv.tv_sec=TIMEOUT_SEC;//超时两秒 
    tv.tv_usec=0;
    if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv))<0){
        perror("setsockopt failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
        printf("[CLIENT] window_size:%d\n",WINDOW_SIZE);
        printf("[CLIENT] line server %s:%d,data...\n",SERVER_IP,SERVER_PORT);
    for(int i=0;i<WINDOW_SIZE;i++){
        send_window[i].acked=1;
    }
    //主循环：发送窗口内包 
    while(base<MAX_SEQ){
        printf("send...:");
        fgets(user_input,BUFFER_SIZE,stdin);
        user_input[strcspn(user_input,"\n")]=0;
        if(strcmp(user_input,"exit")==0){
            printf("[CLIENT]close...\n");
            break;
        }
        //保证next_seq还在发送窗口内部 
        while(next_seq<base+WINDOW_SIZE&&next_seq<MAX_SEQ){
            int idx=next_seq%WINDOW_SIZE;//通过取模定位窗口内位置 
            send_window[idx].seq=next_seq;
            send_window[idx].acked=0;
            send_window[idx].send_time=time(NULL);
            snprintf(send_window[idx].data,BUFFER_SIZE,"%d:%.*s",next_seq,(int)(BUFFER_SIZE-10),user_input);
            int len=strlen(send_window[idx].data);
            int ret=sendto(sockfd,send_window[idx].data,len,0,(struct sockaddr*)&server_addr,sizeof(server_addr));//发送UDP包 
            if(ret>0){
                printf("[CLIENT] send seq=%d,len=%d bytes\n",next_seq,ret);
            }else{
                perror("sendto failed");
            }
            next_seq++;
        }
        //接收ACK与超时处理 
        struct sockaddr_in from_addr;
        socklen_t addr_len=sizeof(from_addr);
        ssize_t n=recvfrom(sockfd,buffer,BUFFER_SIZE,0,(struct sockaddr*)&from_addr,&addr_len);
        if(n<0){
            if(errno==EWOULDBLOCK||errno==EAGAIN){
                printf("[CLIENT] overtime\n");
#ifdef USE_GBN//超时重传整个窗口内未确认的包 
                for(int i=base;i<next_seq;i++){
                    int idx=i%WINDOW_SIZE;
                    if(!send_window[idx].acked){
                        printf("[CLIENT] resend seq=%d\n",send_window[idx].seq); sendto(sockfd,send_window[idx].data,strlen(send_window[idx].data),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
                        send_window[idx].send_time=time(NULL);//更新发送时间 
                     }
                }
#endif
#ifdef USE_SR//选择性重传 
                time_t now=time(NULL);
                for(int i=base;i<next_seq;i++){
                    int idx=i%WINDOW_SIZE;
                    if(!send_window[idx].acked && (now-send_window[idx].send_time)>=TIMEOUT_SEC){
                        printf("[CLIENT] SR seq=%d\n",send_window[idx].seq);
                        sendto(sockfd,send_window[idx].data,strlen(send_window[idx].data),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
                        send_window[idx].send_time=time(NULL);
                        }
                    }
#endif
            }else{
                perror("recvfrom failed");
                break;
            }
        }else{//确认收到ACK 
            buffer[n]='\0';
            printf("[CLIENT] raw received:%s\n",buffer);
            if(strncmp(buffer,"ACK:",4)==0){
                int ack_seq=atoi(buffer+4);
                printf("[CLIENT] success ACK:%d\n",ack_seq);
#ifdef USE_GBN//处理ACK和重复ACK 
                if(ack_seq==last_ack_seq){
                    dup_ack_count++;
                    if(dup_ack_count==3){//收到三个重复ACK，快速重传 
                        printf("[CLIENT] duplicate ACK,fast retransmit seq:%d\n",ack_seq+1);
                        int idx=
                        (ack_seq+1)%WINDOW_SIZE;                           sendto(sockfd,send_window[idx].data,strlen(send_window[idx].data),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
                        send_window[idx].send_time=time(NULL);
                        }
                    }else{
                        last_ack_seq=ack_seq;
                        dup_ack_count=0;
                    }
                    //标记序号已确认，滑动窗口 
                    send_window[ack_seq%WINDOW_SIZE].acked=1;
                    while(base<MAX_SEQ&&send_window[base%WINDOW_SIZE].acked){
                        base++;
                    }
                    printf("[CLIENT] new base=%d\n",base);
#endif
#ifdef USE_SR//处理ACK和重复ACK 
                    if(ack_seq==last_ack_seq){
                        dup_ack_count++;
                        if(dup_ack_count==3){
                            printf("[CLIENT] dup ACK,SR seq:%d\n",ack_seq);
                            int idx=ack_seq%WINDOW_SIZE;
                            sendto(sockfd,send_window[idx].data,strlen(send_window[idx].data),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
                            send_window[idx].send_time=time(NULL);
                            }
                        }else{
                            last_ack_seq=ack_seq;
                            dup_ack_count=1;
                        }
                        //标记序号已确认，滑动窗口 
                        send_window[ack_seq%WINDOW_SIZE].acked=1;
                        while(base<MAX_SEQ && send_window[base%WINDOW_SIZE].acked){
                            printf("[CLIENT] SR base=%d\n",base);
                            base++;//窗口左边界右移 
                            }
#endif
                }
            }
            }
    close(sockfd);
    printf("[CLIENT] exit\n");
    return 0;
}
