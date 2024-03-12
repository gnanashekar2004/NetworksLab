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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#define MESSAGE_SIZE 1024
#define SEQUENCE_NUMBER_BITS 4

#define MAX_MTP_SOCKETS 25

#define ENOTBOUND 60

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

#define T 5
#define Prob 0.05

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
    struct sockaddr_in dest_addr;
    int send_buffer[10][MESSAGE_SIZE];
    int recv_buffer[5][MESSAGE_SIZE];
    int swnd[5];
    int rwnd[5];
}mtp_socket_t;

typedef int semaphore;

extern mtp_socket_t mtp_sockets[MAX_MTP_SOCKETS];

int m_socket(int domain,int type, int protocol);
int m_bind(int mtp_socket_id,struct sockaddr_in src_addr,struct sockaddr_in dest_addr);
int m_sendto(int mtp_socket_id,const void *message,size_t length,struct sockaddr_in dest_addr);
int m_recvfrom(int mtp_socket_id,void *message,size_t length,struct sockaddr_in *src_addr);
int m_close(int mtp_socket_id);
int dropMessage(float p);

#endif