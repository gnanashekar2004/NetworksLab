#include "msocket.h"
#include <pthread.h>

void *receiver(void *arg){
    printf("Receiver thread\n");
    pthread_exit(NULL);
}

void *sender(void *arg){
    printf("Sender thread\n");
    pthread_exit(NULL);
}

int main(){
    int sockid;
    struct sockaddr_in addr;

    pthread_t R,S;
    int rtid,stid;

    
    initialize_shared_memory();
    initialize_semaphores();

    rtid = pthread_create(&R, NULL, receiver, NULL);
    if(rtid){
        printf("Error creating receiver thread\n");
        exit(1);
    }

    stid = pthread_create(&S, NULL, sender, NULL);
    if(stid){
        printf("Error creating sender thread\n");
        exit(1);
    }

    while(1){
        semwait(&semid1);
        if(sock_info->sock_id == 0 && sock_info->IP[0] == '\0' && sock_info->port == 0 && sock_info->Errno == 0){
            sockid = socket(AF_INET, SOCK_DGRAM, 0);
            if(sockid < 0 ){
                sock_info->sock_id = -1;
                sock_info->Errno = errno;
            }else{
                sock_info->sock_id = sockid;
            } 
        }
        else{
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(sock_info->IP);
            addr.sin_port = htons(sock_info->port);
            if(bind(sock_info->sock_id, (struct sockaddr *)&addr, sizeof(addr)) < 0){
                sock_info->sock_id = -1;
                sock_info->Errno = errno;
            }
        }

        semsignal(&semid2);
    }

    pthread_join(R, NULL);
    pthread_join(S, NULL);

    pthread_exit(NULL);

    return 0;
}