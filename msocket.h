#ifndef MSOCKET_H
#define MSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#define MESSAGE_SIZE 1024
#define SEQUENCE_NUMBER_BITS 4

#define SOCK_MTP 1

#define MAX_MTP_SOCKETS 25

#define ENOTBOUND 60

#define T 5
#define Prob 0.05

#define SEM_KEY_PATH "."
#define SEM_KEY_ID1 'A'
#define SEM_KEY_ID2 'B'

#define SHM_KEY_PATH "."
#define SHM_KEY_ID1 'C'
#define SHM_KEY_ID2 'D'

typedef struct {
    int sock_id;
    char IP[16];
    int port;
    int Errno;
} SOCK_INFO;

typedef struct{
    int is_free;
    int process_id;
    int udp_socket_id;
    struct sockaddr_in src_addr;
    struct sockaddr_in dest_addr;
    int send_buffer[10][MESSAGE_SIZE];
    int recv_buffer[5][MESSAGE_SIZE];
    int swnd[5];
    int rwnd[5];
}mtp_socket_t;

#define SHM_SOCK_INFO sizeof(SOCK_INFO)
#define SHM_MTP_SOCKETS (MAX_MTP_SOCKETS*sizeof(mtp_socket_t))

typedef int semaphore;

void semwait(semaphore *semid);
void semsignal(semaphore *semid);
void initialize_semaphores();
void initialize_shared_memory();
int m_socket(int domain,int type, int protocol);
int m_bind(int mtp_socket_id,struct sockaddr_in src_addr,struct sockaddr_in dest_addr);
int m_sendto(int mtp_socket_id,const void *message,size_t length,struct sockaddr_in dest_addr);
int m_recvfrom(int mtp_socket_id,void *message,size_t length,struct sockaddr_in *src_addr);
int m_close(int mtp_socket_id);
int dropMessage(float p);

#endif
