#include "msocket.h"
#include <pthread.h>
#include <sys/select.h>
#include <signal.h>

int semid1, semid2;
SOCK_INFO *shared_sock_info;
mtp_socket_t *shared_mtp;

void R_handle_timeout(){
    for(int i=0;i<MAX_MTP_SOCKETS;i++){
        if(!shared_mtp[i].is_free && shared_mtp[i].rwnd.nospace){
            if(shared_mtp[i].rwnd.size>0){
                //send duplicate ACK to sender with updated size
                shared_mtp[i].rwnd.nospace=0;
            }
            else{
                printf("No space available for socket %d, continue to wait\n",i);
            }
        }
    }    
}

void S_handle_timeout(){
    for(int i=0;i<MAX_MTP_SOCKETS;i++){
        if(!shared_mtp[i].is_free){
            time_t now;
            time(&now);

            if(difftime(now,shared_mtp[i].swnd.last_sent_time)>=TIMEOUT){
                //retransmit messages in swnd.
            }
        }
    }

}

void *receiver(void *arg){
    (void)arg;
    printf("Receiver thread\n");
    fd_set readfds;
    int max_fd;

    char buffer[MESSAGE_SIZE];

    while(1){
        FD_ZERO(&readfds);
        max_fd=0;

        for(int i=0;i<MAX_MTP_SOCKETS;i++){
            if(!shared_mtp[i].is_free){
                FD_SET(shared_mtp[i].udp_socket_id,&readfds);
                if(shared_mtp[i].udp_socket_id>max_fd){
                    max_fd=shared_mtp[i].udp_socket_id;
                }
            }
        }

        struct timeval timeout;
        timeout.tv_sec=TIMEOUT;
        timeout.tv_usec=0;

        int activity = select(max_fd+1,&readfds,NULL,NULL,&timeout);

        if(activity<0){
            perror("select_R");
            continue;
        }else if(activity==0){
            R_handle_timeout();
        }else{
            for(int i=0;i<MAX_MTP_SOCKETS;i++){
                if(!shared_mtp[i].is_free && FD_ISSET(shared_mtp[i].udp_socket_id,&readfds)){
                    memset(buffer,0,sizeof(buffer));
                    //handle recv messages
                    
                    if(buffer[0]=='M'){
                        //data msg

                        //extract seqence no.
                        //store msg in buffer.
                        //update swnd and rwnd
                        //set nospace if buffer full.
                    }
                    else if(buffer[0]=='A'){
                        //ACK msg

                        //extract seq no
                        //update swnd
                        //remove msg from buffer
                    }
                    else if(buffer[0]=='D'){
                        //Duplicate ACK msg

                        //extract seq no
                        //update swnd
                    }
                }
            }
        }
    }

    pthread_exit(NULL);
    return NULL;
}

void *sender(void *arg){
    (void)arg;
    printf("Sender thread\n");

    srand(time(0));

    while(1){
        int sleep_time=rand()%(TIMEOUT/2);
        sleep(sleep_time);

        S_handle_timeout();

        for(int i=0;i<MAX_MTP_SOCKETS;i++){
            if(!shared_mtp[i].is_free){
                //send pending messages.
            }
        }
    }

    pthread_exit(NULL);
    return NULL;
}

int main(){
    int sockid;
    struct sockaddr_in addr;

    pthread_t R,S;
    int rtid,stid;

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