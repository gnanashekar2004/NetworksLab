#include "msocket.h"

#define IP_ADDRESS "127.0.0.2"
#define PORT 8040

#define DEST_IP_ADDRESS "127.0.0.1"
#define DEST_PORT 8080


int main(){

    int sockindex;
    struct sockaddr_in addr,destAddr;

    printf("user 2 socket creating\n");

    sockindex = m_socket(AF_INET, SOCK_MTP, 0);
    if(sockindex < 0){
        printf("Error creating socket\n");
        exit(1);
    }

    printf("user 2 socket created\n");

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    addr.sin_port = htons(PORT);

    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(DEST_IP_ADDRESS);
    destAddr.sin_port = htons(DEST_PORT);

    printf("user 2 socket binding\n");

    if(m_bind(sockindex, addr, destAddr) < 0){
        printf("Error binding socket\n");
        exit(1);
    }

    printf("user 2 socket binded\n");

    return 0;
}