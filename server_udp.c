#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define MAX_SEQ 20
#define WINDOW_SIZE 3
//#define USE_GBN
#define USE_SR//通过预编译切换两种可靠传输 
int main(){
    srand(time(NULL));
    int sockfd;
    struct sockaddr_in server_addr,client_addr;
    socklen_t client_len=sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int expected_seq=0;
    //创建UDP套接字 
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    //绑定窗口 
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(SERVER_PORT);
    if(bind(sockfd,(const struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] success%d,waiting seq=0\n",SERVER_PORT);
#ifdef USE_GBN
    while(1){
        ssize_t n=recvfrom(sockfd,buffer,BUFFER_SIZE,0,(struct sockaddr*)&client_addr,&client_len);
        if(n<0){
            perror("recvfrom failed");
            continue;
        }
        //模拟丢包30%概率 
        int drop_rate=30;
        int rand_val=rand()%100;
        if(rand_val<drop_rate){
            char *colon_ptr=strchr(buffer,':');
            if(colon_ptr !=NULL){
                int seq=atoi(buffer);
                printf("[SERVER] dup %d\n",seq);
            }
            continue;
        }
        buffer[n]='\0';
        char *colon_ptr=strchr(buffer,':');
        if(colon_ptr==NULL) continue;
        int seq=atoi(buffer);
        char *payload=colon_ptr+1;
        printf("[SERVER] receive seq=%d,data=%s\n",seq,payload);
        //若收到期望数据，更新序号 
        if(seq==expected_seq){
            expected_seq++;
            char ack_buf[32];
            snprintf(ack_buf,sizeof(ack_buf)-1,"ACK:%d",seq);
            sendto(sockfd,ack_buf,strlen(ack_buf),0,(struct sockaddr*)&client_addr,client_len);
            printf("[server] ACK:%d,next seq=%d\n",seq,expected_seq);
        }else{//收到乱序包，直接丢弃，回复上一个ACK 
            printf("[SERVER] seq=%d,reply ACK:%d\n",seq,expected_seq-1);
            if(expected_seq>0){
                char ack_buf[32];
                snprintf(ack_buf,sizeof(ack_buf),"ACK:%d",expected_seq-1);
                sendto(sockfd,ack_buf,strlen(ack_buf),0,(struct sockaddr*)&client_addr,client_len);
                printf("[SERVER]reply ACK:%d\n",expected_seq-1);
            }
        }//所有包接收完成，退出循环 
        if(expected_seq>=MAX_SEQ){
            printf("[SERVER] finish\n");
            break;
        }
    }
#endif
#ifdef USE_SR
    int recv_base=0;
    int recv_acked[WINDOW_SIZE]={0};//用于确认窗口内包是否已确认 
    while(1){
      ssize_t n=recvfrom(sockfd,buffer,BUFFER_SIZE,0,(struct sockaddr*)&client_addr,&client_len);
      if(n<0){
          perror("recvfrom failed");
          continue;
      }
      //模拟丢包 
      int drop_rate=30;
      int rand_val=rand()%100;
      if(rand_val<drop_rate){
          char *colon_ptr=strchr(buffer,':');
          if(colon_ptr !=NULL){
              int seq=atoi(buffer);
              printf("[SERVER] dup %d\n",seq);
          }
          continue;
      }
      buffer[n]='\0';
      char *colon_ptr=strchr(buffer,':');
      if(colon_ptr==NULL) continue;
      int seq=atoi(buffer);
      char *payload=colon_ptr+1;
      printf("[SERVER] SR seq=%d,data=%s\n",seq,payload);
      if(seq>=recv_base&&seq<recv_base+WINDOW_SIZE){//序号在窗口内 
          int idx=seq%WINDOW_SIZE;
          if(!recv_acked[idx]){
              recv_acked[idx]=1;//标记序号已收到 
          }
          char ack_buf[32];
          sprintf(ack_buf,"ACK=%d",seq);
          sendto(sockfd,ack_buf,strlen(ack_buf),0,(struct sockaddr*)&client_addr,client_len);
          printf("[SERVER] SR reply ACK=%d",seq);//对所有包回复ACK 
          //向前滑动窗口 
          while(recv_acked[recv_base%WINDOW_SIZE]){
              printf("[SERVER] SR deliver seq=%d\n",recv_base);
              recv_acked[recv_base%WINDOW_SIZE]=0;
              recv_base++;
              if(recv_base>=MAX_SEQ){
                  printf("[SERVER] SR finish\n");
                  close(sockfd);
                  return 0;
              }
          }
      }
      else if(seq<recv_base){//收到旧包，重新回复ACK 
          char ack_buf[32];
          sprintf(ack_buf,"ACK=%d",seq);
          sendto(sockfd,ack_buf,strlen(ack_buf),0,(struct sockaddr*)&client_addr,client_len);
          printf("[SERVER] duplicate seq=%d,ACK=%d\n",seq,seq);
      }
  }                 
#endif
    close(sockfd);
    return 0;
}
