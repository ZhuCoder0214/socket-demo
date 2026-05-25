#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
//#define USE_GBN
#define USE_SR
#define SERVER_IP "192.168.56.103"
#define SERVER_PORT 12345
#define BUFFER_SIZE 2048
#define TIMEOUT_SEC 2
#define WINDOW_SIZE 3
#define MAX_SEQ 20
#define QUEUE_SIZE 1024
#define THREAD_NUM 4
typedef struct{
    char data[BUFFER_SIZE];
    struct sockaddr_in server_addr;
}SendTask;
typedef struct{
    SendTask buf[QUEUE_SIZE];
    atomic_uint head;
    atomic_uint tail;
}LockFreeQueue;
LockFreeQueue send_queue;
int sockfd;
volatile int running=1;
void queue_init(LockFreeQueue *q){
    atomic_init(&q->head,0);
    atomic_init(&q->tail,0);
}
int queue_enqueue(LockFreeQueue *q,const SendTask *task){
    uint32_t tail=atomic_load(&q->tail);
    uint32_t head=atomic_load(&q->head);
    uint32_t next_tail=(tail+1)%QUEUE_SIZE;
    //printf("[DEBUG] head=%u,tail=%u\n",head,tail);
    //printf("[DEBUG] next_tail=%u\n",next_tail);
    if(next_tail==atomic_load(&q->head)){
      //  printf("[DEBUG] full");
        return -1;
    }
    q->buf[tail]=*task;
    atomic_store(&q->tail,next_tail);
    //printf("[DEBUG] enter success,new tail=%u\n",next_tail);
    return 0;
}
int queue_dequeue(LockFreeQueue *q,SendTask *task){
    uint32_t head=atomic_load(&q->head);
    if(head==atomic_load(&q->tail)){
        return -1;
    }
    *task=q->buf[head];
    atomic_store(&q->head,(head+1)%QUEUE_SIZE);
    return 0;
}
void *consumer_thread(void *arg){
    (void)arg;
    SendTask task;
    printf("success!\n");
    while(running){
        if(queue_dequeue(&send_queue,&task)==0){
            printf("[THREAD %lu] pack:%s\n",pthread_self(),task.data);
            int ret=sendto(sockfd,task.data,strlen(task.data),0,(struct sockaddr *)&task.server_addr,sizeof(task.server_addr));
            if(ret<0){
                perror("sendto failed");
            }else{
                printf("[THREAD %lu] send success!bytes:%d\n",pthread_self(),ret);
            }
        }else{
            usleep(1000);
        }
    }
    return NULL;
}
typedef struct{
    int seq;
    int acked;
    char data[BUFFER_SIZE];
    time_t send_time;
}Packet;
Packet send_window[WINDOW_SIZE];
int base=0;
int next_seq=0;

int main(){
    
    char send_buf[BUFFER_SIZE];
   
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char user_input[BUFFER_SIZE];
    int dup_ack_count=0;
    int last_ack_seq=-1;
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT);
  
    queue_init(&send_queue);
    pthread_t threads[THREAD_NUM];
    for(int i=0;i<THREAD_NUM;i++){
        pthread_create(&threads[i],NULL,consumer_thread,NULL);
    }
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
    for(int i=0;i<WINDOW_SIZE;i++){
        send_window[i].acked=1;
    }
    while(base<MAX_SEQ){
        printf("send...:");
        fgets(user_input,BUFFER_SIZE,stdin);
        user_input[strcspn(user_input,"\n")]=0;
        if(strcmp(user_input,"exit")==0){
            printf("[CLIENT]close...\n");
            break;
        }
        while(next_seq<base+WINDOW_SIZE&&next_seq<MAX_SEQ){
            int idx=next_seq%WINDOW_SIZE;
            send_window[idx].seq=next_seq;
            send_window[idx].acked=0;
            send_window[idx].send_time=time(NULL);
            snprintf(send_window[idx].data,BUFFER_SIZE,"%d:%.*s",next_seq,(int)(BUFFER_SIZE-10),user_input);
            int len=strlen(send_window[idx].data);
            SendTask task;
            strcpy(task.data,send_window[idx].data);
            task.server_addr=server_addr;
            while(queue_enqueue(&send_queue,&task)!=0){
                usleep(1000);
            }
            printf("[CLIENT] send seq=%d,len=%d\n",next_seq,len);
           
            next_seq++;
        }
        struct sockaddr_in from_addr;
        socklen_t addr_len=sizeof(from_addr);
        ssize_t n=recvfrom(sockfd,buffer,BUFFER_SIZE,0,(struct sockaddr*)&from_addr,&addr_len);
        if(n<0){
            if(errno==EWOULDBLOCK||errno==EAGAIN){
                printf("[CLIENT] overtime\n");
#ifdef USE_GBN
                for(int i=base;i<next_seq;i++){
                    int idx=i%WINDOW_SIZE;
                    if(!send_window[idx].acked){
                        printf("[CLIENT] resend seq=%d\n",send_window[idx].seq); 
                        SendTask task;
                        strcpy(task.data,send_window[idx].data);
                        task.server_addr=server_addr;
                        while(queue_enqueue(&send_queue,&task)!=0){
                            usleep(1000);
                        }
            
                        send_window[idx].send_time=time(NULL);
                     }
                }
#endif
#ifdef USE_SR
                time_t now=time(NULL);
                for(int i=base;i<next_seq;i++){
                    int idx=i%WINDOW_SIZE;
                    if(!send_window[idx].acked && (now - send_window[idx].send_time)>=TIMEOUT_SEC){
                        printf("[CLIENT] SR seq=%d\n",send_window[idx].seq);
                        SendTask task;
                        strcpy(task.data,send_window[idx].data);
                        task.server_addr=server_addr;
                        while(queue_enqueue(&send_queue,&task)!=0){
                        usleep(1000);
                    }
                        send_window[idx].send_time=time(NULL);
                        }
                    }
#endif
            }else{
                perror("recvfrom failed");
                break;
            }
        }else{
            buffer[n]='\0';
            printf("[CLIENT] raw received:%s\n",buffer);
            if(strncmp(buffer,"ACK:",4)==0){
                int ack_seq=atoi(buffer+4);
                printf("[CLIENT] success ACK:%d\n",ack_seq);
#ifdef USE_GBN
                if(ack_seq==last_ack_seq){
                    dup_ack_count++;
                    if(dup_ack_count==3){
                        printf("[CLIENT] duplicate ACK,fast retransmit seq:%d\n",ack_seq+1);
                        int idx=
                        (ack_seq+1)%WINDOW_SIZE;                          
                        SendTask task;
                        strcpy(task.data,send_window[idx].data);
                        task.server_addr=server_addr;
                        while(queue_enqueue(&send_queue,&task)!=0){
                            usleep(1000);
                        }
                        send_window[idx].send_time=time(NULL);
                        }
                    }else{
                        last_ack_seq=ack_seq;
                        dup_ack_count=0;
                    }
                    send_window[ack_seq%WINDOW_SIZE].acked=1;
                    while(base<MAX_SEQ&&send_window[base%WINDOW_SIZE].acked){
                        base++;
                    }
                    printf("[CLIENT] new base=%d\n",base);
#endif
#ifdef USE_SR
                    if(ack_seq==last_ack_seq){
                        dup_ack_count++;
                        if(dup_ack_count==3){
                            printf("[CLIENT] dup ACK,SR seq:%d\n",ack_seq);
                            int idx=ack_seq%WINDOW_SIZE;
                            SendTask task;
                            strcpy(task.data,send_window[idx].data);
                            task.server_addr=server_addr;
                            while(queue_enqueue(&send_queue,&task)!=0){
                                usleep(1000);
                            }
                            send_window[idx].send_time=time(NULL);
                            }
                        }else{
                            last_ack_seq=ack_seq;
                            dup_ack_count=1;
                        }
                        send_window[ack_seq%WINDOW_SIZE].acked=1;
                        while(base<MAX_SEQ && send_window[base%WINDOW_SIZE].acked){
                            printf("[CLIENT] SR base=%d\n",base);
                            base++;
                            }
#endif
                }
            }
            sleep(1);
            running=0;
            for(int i=0;i<THREAD_NUM;i++){
                pthread_join(threads[i],NULL);
            }
    close(sockfd);
    printf("[CLIENT] exit\n");
    return 0;
}
}
