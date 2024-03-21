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

    int semid1, semid2;
    SOCK_INFO *shared_sock_info;
    mtp_socket_t *shared_mtp;

    initialize_semaphores(&semid1, &semid2);
    initialize_shared_memory(&shared_sock_info, &shared_mtp);

    semctl(semid1, 0, SETVAL, 0);
    semctl(semid2, 0, SETVAL, 0);

    memset(shared_sock_info, 0, sizeof(SOCK_INFO));

    for(int i = 0; i < MAX_MTP_SOCKETS; i++){
        shared_mtp[i].is_free = 1;
        shared_mtp[i].process_id = -1;
        shared_mtp[i].udp_socket_id = -1;
    }

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
        if(shared_sock_info->sock_id == 0 && shared_sock_info->IP[0] == '\0' && shared_sock_info->port == 0 && shared_sock_info->Errno == 0){
            sockid = socket(AF_INET, SOCK_DGRAM, 0);
            if(sockid < 0 ){
                shared_sock_info->sock_id = -1;
                shared_sock_info->Errno = errno;
            }else{
                shared_sock_info->sock_id = sockid;
            } 
        }
        else{
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(shared_sock_info->IP);
            addr.sin_port = htons(shared_sock_info->port);
            if(bind(shared_sock_info->sock_id, (struct sockaddr *)&addr, sizeof(addr)) < 0){
                shared_sock_info->sock_id = -1;
                shared_sock_info->Errno = errno;
            }
        }

        semsignal(&semid2);
    }

    pthread_join(R, NULL);
    pthread_join(S, NULL);

    pthread_exit(NULL);

    return 0;
}