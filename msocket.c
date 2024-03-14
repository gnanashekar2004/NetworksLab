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

void initialize_semaphores()
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

    semid1 = semget(key1, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid1 == -1)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    semid2 = semget(key2, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid2 == -1)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }
}

void initialize_shared_memory(){
    key_t key1=ftok(SHM_KEY_PATH,SHM_KEY_ID1);
    if(key1==-1){
        perror("ftok shared memory key1");
        exit(EXIT_FAILURE);
    }

    key_t key2=ftok(SHM_KEY_PATH,SHM_KEY_ID2);
    if(key2==-1){
        perror("ftok shared memory key2");
        exit(EXIT_FAILURE);
    }

    shmid_sock_info = shmget(key1,SHM_SOCK_INFO,IPC_CREAT|IPC_EXCL|0666);
    if(shmid_sock_info==-1){
        perror("shmget key1");
        exit(EXIT_FAILURE);
    }

    shmid_mtp = shmget(key2,SHM_MTP_SOCKETS,IPC_CREAT|IPC_EXCL|0666);
    if(shmid_mtp==-1){
        perror("shmget key2");
        exit(EXIT_FAILURE);
    }

    shared_memory_sock_info = shmat(shmid_sock_info,NULL,0);
    if(shared_memory_sock_info==(void *)-1){
        perror("shmat key1");
        exit(EXIT_FAILURE);
    }

    shared_memory_mtp = shmat(shmid_mtp,NULL,0);
    if(shared_memory_mtp==(void *)-1){
        perror("shmat key2");
        exit(EXIT_FAILURE);
    }

    sock_info = (SOCK_INFO *)shared_memory_sock_info;
    shared_mtp_sockets = (mtp_socket_t *)shared_memory_mtp;
}

int m_socket(int domain, int type, int protocol)
{
    int i, index = -1;
    for (i = 0; i < MAX_MTP_SOCKETS; i++)
    {
        if (shared_mtp_sockets[i].is_free == 1)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        errno = ENOBUFS;
        return -1;
    }

    memset(&sock_info, 0, sizeof(SOCK_INFO));

    semsignal(&semid1);
    semwait(&semid2);

    if (sock_info->sock_id == -1)
    {
        errno = EIO;
        return -1;
    }

    shared_mtp_sockets[index].udp_socket_id = sock_info->sock_id;

    memset(&sock_info, 0, sizeof(SOCK_INFO));

    return index;
}

int m_bind(int mtp_socket_id, struct sockaddr_in src_addr, struct sockaddr_in dest_addr)
{
    int index = -1;
    for(int i=0;i<MAX_MTP_SOCKETS;i++){
        if(shared_mtp_sockets[i].udp_socket_id==shared_mtp_sockets[mtp_socket_id].udp_socket_id){
            index = i;
            break;
        }
    }
    if(index==-1){
        errno = EINVAL;
        return -1;
    }
    int udp_socket_id = shared_mtp_sockets[index].udp_socket_id;

    sock_info->sock_id = udp_socket_id;
    memcpy(&sock_info->IP, &src_addr.sin_addr.s_addr, sizeof(sock_info->IP));
    sock_info->port = src_addr.sin_port;
    sock_info->Errno = 0;

    shared_mtp_sockets[index].dest_addr = dest_addr;

    semsignal(&semid1);
    semwait(&semid2);

    if(sock_info->sock_id==-1){
        errno = sock_info->Errno;
        return -1;
    }

    memset(&sock_info,0,sizeof(SOCK_INFO));

    return 0;
}

int m_sendto(int mtp_socket_id, const void *message, size_t length, struct sockaddr_in dest_addr)
{
    if(mtp_socket_id<0 || mtp_socket_id>=MAX_MTP_SOCKETS || shared_mtp_sockets[mtp_socket_id].is_free){
        errno = EBADF;
        return -1;
    }

    if(memcmp(&dest_addr,&shared_mtp_sockets[mtp_socket_id].dest_addr,sizeof(struct sockaddr_in))!=0){
        errno = ENOTBOUND;
        return -1;
    }

    int i,index=-1;
    for(i=0;i<10;i++){
        if(shared_mtp_sockets[mtp_socket_id].send_buffer[i][0]==0){
            index = i;
            break;
        }
    }

    if(index==-1){
        errno = ENOBUFS;
        return -1;
    }

    memcpy(shared_mtp_sockets[mtp_socket_id].send_buffer[index],message,length);

    return 0;
}

int m_recvfrom(int mtp_socket_id, void *message, size_t length, struct sockaddr_in *src_addr)
{
    if(shared_mtp_sockets[mtp_socket_id].recv_buffer[0][0]==0)
    {
        errno = ENOMSG;
        return -1;
    }

    memcpy(message, shared_mtp_sockets[mtp_socket_id].recv_buffer[0],length);
    
    for(int i=0;i<4;i++){
        memcpy(&shared_mtp_sockets[mtp_socket_id].recv_buffer[i][0],&shared_mtp_sockets[mtp_socket_id].recv_buffer[i+1][0],sizeof(shared_mtp_sockets[mtp_socket_id].recv_buffer[i]));
    }

    memset(&shared_mtp_sockets[mtp_socket_id].recv_buffer[4][0],0,sizeof(shared_mtp_sockets[mtp_socket_id].recv_buffer[4]));

    src_addr->sin_addr.s_addr = shared_mtp_sockets[mtp_socket_id].dest_addr.sin_addr.s_addr;
    src_addr->sin_port = shared_mtp_sockets[mtp_socket_id].dest_addr.sin_port;

    return 0;
}

int m_close(int mtp_socket_id)
{
    close(shared_mtp_sockets[mtp_socket_id].udp_socket_id);

    shared_mtp_sockets[mtp_socket_id].is_free = 1;
    shared_mtp_sockets[mtp_socket_id].udp_socket_id = -1;
    memset(&shared_mtp_sockets[mtp_socket_id].dest_addr, 0, sizeof(struct sockaddr_in));

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