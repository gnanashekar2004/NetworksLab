#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include "msocket.h"

extern semaphore semid1 = 0;
extern semaphore semid2 = 0;

SOCK_INFO sock_info = {0};

mtp_socket_t mtp_sockets[MAX_MTP_SOCKETS];

int m_socket(int domain, int type, int protocol)
{
    int i, index = -1;
    for (i = 0; i < MAX_MTP_SOCKETS; i++)
    {
        if (mtp_sockets[i].is_free == 1)
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

    up(&semid1);
    down(&semid2);

    if (sock_info.sock_id == -1)
    {
        errno = EIO;
        return -1;
    }

    mtp_sockets[index].udp_socket_id = sock_info.sock_id;

    memset(&sock_info, 0, sizeof(SOCK_INFO));

    return index;
}

int m_bind(int mtp_socket_id, struct sockaddr_in src_addr, struct sockaddr_in dest_addr)
{
    int udp_socket_id = mtp_sockets[mtp_socket_id].udp_socket_id;

    SOCK_INFO sock_info = {udp_socket_id,src_addr.sin_addr.s_addr,src_addr.sin_port,0};

    up(&semid1);
    down(&semid2);

    if(sock_info.sock_id==-1){
        errno = sock_info.Errno;
        return -1;
    }

    memset(&sock_info,0,sizeof(SOCK_INFO));

    return 0;
}

int m_sendto(int mtp_socket_id, const void *message, size_t length, struct sockaddr_in dest_addr)
{
    if(mtp_socket_id<0 || mtp_socket_id>=MAX_MTP_SOCKETS || mtp_sockets[mtp_socket_id].is_free){
        errno = EBADF;
        return -1;
    }

    if(memcmp(&dest_addr,&mtp_sockets[mtp_socket_id].dest_addr,sizeof(struct sockaddr_in))!=0){
        errno = ENOTBOUND;
        return -1;
    }

    int i,index=-1;
    for(i=0;i<10;i++){
        if(mtp_sockets[mtp_socket_id].send_buffer[i][0]==0){
            index = i;
            break;
        }
    }

    if(index==-1){
        errno = ENOBUFS;
        return -1;
    }

    memcpy(mtp_sockets[mtp_socket_id].send_buffer[index],message,length);

    return 0;
}

int m_recvfrom(int mtp_socket_id, void *message, size_t length, struct sockaddr_in *src_addr)
{
    if(mtp_sockets[mtp_socket_id].recv_buffer[0][0]==0)
    {
        errno = ENOMSG;
        return -1;
    }

    memcpy(message, mtp_sockets[mtp_socket_id].recv_buffer[0],length);
    
    for(int i=0;i<4;i++){
        memcpy(&mtp_sockets[mtp_socket_id].recv_buffer[i][0],&mtp_sockets[mtp_socket_id].recv_buffer[i+1][0],sizeof(mtp_sockets[mtp_socket_id].recv_buffer[i]));
    }

    memset(&mtp_sockets[mtp_socket_id].recv_buffer[4][0],0,sizeof(mtp_sockets[mtp_socket_id].recv_buffer[4]));

    src_addr->sin_addr.s_addr = mtp_sockets[mtp_socket_id].dest_addr.sin_addr.s_addr;
    src_addr->sin_port = mtp_sockets[mtp_socket_id].dest_addr.sin_port;

    return 0;
}

int m_close(int mtp_socket_id)
{
    close(mtp_sockets[mtp_socket_id].udp_socket_id);

    mtp_sockets[mtp_socket_id].is_free = 1;
    mtp_sockets[mtp_socket_id].udp_socket_id = -1;
    memset(&mtp_sockets[mtp_socket_id].dest_addr, 0, sizeof(struct sockaddr_in));

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