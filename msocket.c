#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include "msocket.h"


void semwait (semaphore *semid)
{
   struct sembuf sb;
   int id=*semid;

   sb.sem_num = 0;
   sb.sem_flg = 0;
   sb.sem_op = -1;
   semop(id, &sb, 1);
}

void semsignal (semaphore *semid)
{
   struct sembuf sb;
   int id=*semid;

   sb.sem_num = 0;
   sb.sem_flg = 0;
   sb.sem_op = 1;
   semop(id, &sb, 1);
}

void initialize_semaphores(int *semid1, int *semid2)
{
    key_t key1 = ftok(SEM_KEY_PATH, SEM_KEY_ID1);
    if (key1 == -1)
    {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    key_t key2 = ftok(SEM_KEY_PATH, SEM_KEY_ID2);
    if (key2 == -1)
    {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    *semid1 = semget(key1, 1, IPC_CREAT | 0666);
    if (*semid1 == -1)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    *semid2 = semget(key2, 1, IPC_CREAT | 0666);
    if (*semid2 == -1)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }
}

void initialize_shared_memory(SOCK_INFO **shared_sock_info, mtp_socket_t **shared_mtp) {
    key_t key1 = ftok(SHM_KEY_PATH, SHM_KEY_ID1);
    if (key1 == -1) {
        perror("ftok shared memory key");
        exit(EXIT_FAILURE);
    }

    key_t key2 = ftok(SHM_KEY_PATH, SHM_KEY_ID2);
    if (key2 == -1) {
        perror("ftok shared memory key");
        exit(EXIT_FAILURE);
    }

    int shmid1 = shmget(key1, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    if (shmid1 == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    int shmid2 = shmget(key2, sizeof(mtp_socket_t) * MAX_MTP_SOCKETS, IPC_CREAT | 0666);
    if (shmid2 == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    *shared_sock_info = (SOCK_INFO *)shmat(shmid1, NULL, 0);
    if (*shared_sock_info == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    *shared_mtp = (mtp_socket_t *)shmat(shmid2, NULL, 0);
    if (*shared_mtp == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
}



int m_socket(int domain, int type, int protocol)
{
    printf("m_socket\n");
    int semid1, semid2;
    SOCK_INFO *shared_sock_info;
    mtp_socket_t *shared_mtp;

    initialize_semaphores(&semid1, &semid2);
    initialize_shared_memory(&shared_sock_info, &shared_mtp);

    int i, index = -1;
    for (i = 0; i < MAX_MTP_SOCKETS; i++)
    {
        if (shared_mtp[i].is_free == 1)
        {
            index = i;
            break;
        }
    }

    printf("index: %d\n",index);

    if (index == -1)
    {
        errno = ENOBUFS;
        return -1;
    }

    memset(&shared_sock_info, 0, sizeof(SOCK_INFO));

    semsignal(&semid1);
    semwait(&semid2);


    if (shared_sock_info->sock_id == -1)
    {
        errno = EIO;
        return -1;
    }

    // printf("hi\n");
    // shared_mtp[index].udp_socket_id = shared_sock_info->sock_id;
    // printf("shared_sock_info: %d\n", shared_sock_info->sock_id);
    printf("shared_mtp: %d\n", shared_mtp[index].udp_socket_id);
    // printf("hi\n");
    printf("Socked created with id %d\n", shared_sock_info->sock_id);

    memset(&shared_sock_info, 0, sizeof(SOCK_INFO));

    return index;
}

int m_bind(int index, struct sockaddr_in src_addr, struct sockaddr_in dest_addr)
{
    printf("m_bind\n");
    int semid1, semid2;
    SOCK_INFO *shared_sock_info;
    mtp_socket_t *shared_mtp;

    initialize_semaphores(&semid1, &semid2);
    initialize_shared_memory(&shared_sock_info, &shared_mtp);

    int udp_socket_id = shared_mtp[index].udp_socket_id;

    shared_sock_info->sock_id = udp_socket_id;
    memcpy(&shared_sock_info->IP, &src_addr.sin_addr.s_addr, sizeof(shared_sock_info->IP));
    shared_sock_info->port = src_addr.sin_port;
    shared_sock_info->Errno = 0;

    shared_mtp[index].dest_addr = dest_addr;

    semsignal(&semid1);
    semwait(&semid2);

    if(shared_sock_info->sock_id==-1){
        errno = shared_sock_info->Errno;
        return -1;
    }

    printf("Socket bound with id %d\n",shared_sock_info->sock_id);
    memset(&shared_sock_info,0,sizeof(SOCK_INFO));

    return 0;
}

int m_sendto(int mtp_socket_id, const void *message, size_t length, struct sockaddr_in dest_addr)
{
    int semid1, semid2;
    SOCK_INFO *shared_sock_info;
    mtp_socket_t *shared_mtp;

    initialize_semaphores(&semid1, &semid2);
    initialize_shared_memory(&shared_sock_info, &shared_mtp);

    if(mtp_socket_id<0 || mtp_socket_id>=MAX_MTP_SOCKETS || shared_mtp[mtp_socket_id].is_free){
        errno = EBADF;
        return -1;
    }

    if(memcmp(&dest_addr,&shared_mtp[mtp_socket_id].dest_addr,sizeof(struct sockaddr_in))!=0){
        errno = ENOTBOUND;
        return -1;
    }

    int i,index=-1;
    for(i=0;i<10;i++){
        if(shared_mtp[mtp_socket_id].send_buffer[i][0]==0){
            index = i;
            break;
        }
    }

    if(index==-1){
        errno = ENOBUFS;
        return -1;
    }

    memcpy(shared_mtp[mtp_socket_id].send_buffer[index],message,length);

    return 0;
}

int m_recvfrom(int mtp_socket_id, void *message, size_t length, struct sockaddr_in *src_addr)
{
    int semid1, semid2;
    SOCK_INFO *shared_sock_info;
    mtp_socket_t *shared_mtp;

    initialize_semaphores(&semid1, &semid2);
    initialize_shared_memory(&shared_sock_info, &shared_mtp);


    if(shared_mtp[mtp_socket_id].recv_buffer[0][0]==0)
    {
        errno = ENOMSG;
        return -1;
    }

    memcpy(message, shared_mtp[mtp_socket_id].recv_buffer[0],length);
    
    for(int i=0;i<4;i++){
        memcpy(&shared_mtp[mtp_socket_id].recv_buffer[i][0],&shared_mtp[mtp_socket_id].recv_buffer[i+1][0],sizeof(shared_mtp[mtp_socket_id].recv_buffer[i]));
    }

    memset(&shared_mtp[mtp_socket_id].recv_buffer[4][0],0,sizeof(shared_mtp[mtp_socket_id].recv_buffer[4]));

    src_addr->sin_addr.s_addr = shared_mtp[mtp_socket_id].dest_addr.sin_addr.s_addr;
    src_addr->sin_port = shared_mtp[mtp_socket_id].dest_addr.sin_port;

    return 0;
}

int m_close(int mtp_socket_id)
{
    int semid1, semid2;
    SOCK_INFO *shared_sock_info;
    mtp_socket_t *shared_mtp;

    initialize_semaphores(&semid1, &semid2);
    initialize_shared_memory(&shared_sock_info, &shared_mtp);



    close(shared_mtp[mtp_socket_id].udp_socket_id);

    shared_mtp[mtp_socket_id].is_free = 1;
    shared_mtp[mtp_socket_id].udp_socket_id = -1;
    memset(&shared_mtp[mtp_socket_id].dest_addr, 0, sizeof(struct sockaddr_in));

    return 0;
}

int dropMessage(float p)
{
    float rand_num = (float)rand()/RAND_MAX;
    if(rand_num<p){
        return 1;
    }
    return 0;
}