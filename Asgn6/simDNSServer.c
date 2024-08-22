#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/ioctl.h>
// #include <linux/if.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <time.h>

#define BUF_SIZE 1024
#define MAX_QUERIES 8
#define MAX_INPUT_SIZE 400
#define MAX_QUERY_SIZE 32
#define PROTOCOL 254
#define TTL 64

#define INTERFACE "wlp2s0"

int p = 0.3;

typedef struct query{
    uint16_t ID;
    uint8_t type;
    uint8_t count;
    uint32_t size[MAX_QUERIES];
    char query[MAX_QUERIES][MAX_QUERY_SIZE];
} query;

typedef struct response{
    uint16_t ID;
    uint8_t type;
    uint8_t count;
    // uint32_t size[MAX_QUERIES];
    uint8_t flag[MAX_QUERIES];
    uint32_t ipaddr[MAX_QUERIES];
} response;

int dropMessage(float p) {
    float r = (float)rand() / RAND_MAX;
    if (r < p) {
        printf("Drop\n");
        return 1;  // Drop
    } else {
        printf("Didn't drop\n");
        return 0;  // Don't drop
    }
}

void fetchInterface(int sockfd, struct ifreq *if1, struct ifreq *if2, struct ifreq *if3){
    strncpy(if1->ifr_name, INTERFACE, IFNAMSIZ-1);
    strncpy(if2->ifr_name, INTERFACE, IFNAMSIZ-1);
    strncpy(if3->ifr_name, INTERFACE, IFNAMSIZ-1);
    // printf("Hi\n");
    // if(ioctl(sockfd, SIOCGIFHWDDR, if1) < 0){
    //     perror("SIOCGIFINDEX");
    //     exit(1);
    // }
    if(ioctl(sockfd, SIOCGIFINDEX, if1) < 0){
        perror("SIOCGIFINDEX");exit(1);
    }
    // printf("Index sucesss\n");
    if(ioctl(sockfd, SIOCGIFADDR, if2) < 0){
        perror("SIOCGIFADDR");exit(1);
    }
    if(ioctl(sockfd, SIOCGIFHWADDR, if3) < 0){
        perror("SIOCGIFHWADDR");exit(1);
    }
    // printf("message\n");
    if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, INTERFACE, IFNAMSIZ-1) < 0){
        perror("SO_BINDTODEVICE");exit(1);
    }
    // printf("end of function\n");
}

uint16_t findCheckSum(void *buffer, int length){
    uint32_t sum = 0;
    uint16_t *buf = buffer;

    while(length > 1){
        sum += *buf++;
        length -= 2;
    }

    if(length == 1){
        sum += *(uint8_t *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (uint16_t)~sum;
}

void setUpHeaders(struct ethhdr **eth_hdr, struct iphdr **ip_hdr, char *sendbuf, char *recvbuf, struct sockaddr_ll *saddr, struct sockaddr_ll *daddr, struct ifreq *index, struct ifreq *ip, struct ifreq *mac);
void setUpResponse(response *res, query *qry, struct iphdr *recv_ip_hdr, struct ethhdr *recv_eth_hdr, struct ethhdr *eth_hdr, struct iphdr *ip_hdr, struct sockaddr_ll *saddr, struct sockaddr_ll *daddr);

int main(int argc, char *argv[]){
    srand(time(NULL));

    int sockfd;
    struct ifreq index;
    struct ifreq ip;
    struct ifreq mac;

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sockfd < 0){
        perror("socket creation error");return 1;
    }
    printf("Socket created\n");

    fetchInterface(sockfd, &index, &ip, &mac);
    // printf("Interface index: %d\n", index.ifr_ifindex);
    // printf("Interface IP: %s\n", inet_ntoa(((struct sockaddr_in *)&ip.ifr_addr)->sin_addr));
    // printf("Interface MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", (unsigned char)mac.ifr_hwaddr.sa_data[0], (unsigned char)mac.ifr_hwaddr.sa_data[1], (unsigned char)mac.ifr_hwaddr.sa_data[2], (unsigned char)mac.ifr_hwaddr.sa_data[3], (unsigned char)mac.ifr_hwaddr.sa_data[4], (unsigned char)mac.ifr_hwaddr.sa_data[5]);

    struct sockaddr_ll saddr;
    struct sockaddr_ll daddr;

    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = index.ifr_ifindex;
    daddr.sll_ifindex = index.ifr_ifindex;
    daddr.sll_halen = ETH_ALEN;

    if(bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0){
        perror("bind error");return 1;
    }
    printf("Bind successful\n");

    struct ethhdr *eth_hdr;
    struct iphdr *ip_hdr;
    char sendbuf[BUF_SIZE];
    char recvbuf[BUF_SIZE];

    setUpHeaders(&eth_hdr,&ip_hdr, sendbuf, recvbuf, &saddr, &daddr,&index, &ip, &mac);
    // printf("Ethernet Header:\n");
    // printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", eth_hdr->h_dest[0], eth_hdr->h_dest[1], eth_hdr->h_dest[2], eth_hdr->h_dest[3], eth_hdr->h_dest[4], eth_hdr->h_dest[5]);
    // printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", eth_hdr->h_source[0], eth_hdr->h_source[1], eth_hdr->h_source[2], eth_hdr->h_source[3], eth_hdr->h_source[4], eth_hdr->h_source[5]);

    // printf("IP Header:\n");
    // printf("Version: %d\n", ip_hdr->version);
    // printf("Header Length: %d\n", ip_hdr->ihl * 4);
    // printf("Type of Service: %d\n", ip_hdr->tos);
    // printf("Total Length: %d\n", ntohs(ip_hdr->tot_len));
    // printf("Identification: %d\n", ntohs(ip_hdr->id));
    // printf("Time to Live: %d\n", ip_hdr->ttl);
    // printf("Protocol: %d\n", ip_hdr->protocol);
    // printf("Source IP: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->saddr));
    // printf("Destination IP: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->daddr));
    // printf("Reached here\n");

    response *res = (response *)(sendbuf + sizeof(struct ethhdr) + sizeof(struct iphdr));
    // res->ID = 0;
    // res->type = 0;
    // res->count = 1;
    // res->flag[0] = 1;
    while(1){
        memset(recvbuf, 0, BUF_SIZE);
        int n = recvfrom(sockfd, recvbuf, BUF_SIZE, 0, NULL, NULL);
        if(n < 0){
            perror("recvfrom error");return 1;
        }
        // printf("Received %d bytes\n", n);
        struct ethhdr *recv_eth_hdr = (struct ethhdr *)recvbuf;
        struct iphdr *recv_ip_hdr = (struct iphdr *)(recvbuf + sizeof(struct ethhdr));
        query *qry = (query *)(recvbuf + sizeof(struct ethhdr) + sizeof(struct iphdr));
        if(recv_ip_hdr->protocol!=254){
            // printf("Not a query: %d\n",recv_ip_hdr->protocol);
            continue;
        }
        // printf("query: %s\n",recvbuf);
        int drop = dropMessage(p);
        if(drop==1){
            printf("Dropped query from client\n");
            continue;
        }
        printf("Received query from client\n");

        // printf("ID: %d\n", qry->ID);
        // printf("Type: %d\n", qry->type);
        // printf("Count: %d\n", qry->count);
        for(int i=0;i<qry->count;i++){
            printf("Query %d: %s\n", i, qry->query[i]);
        }

        setUpResponse(res, qry,recv_ip_hdr,recv_eth_hdr, eth_hdr,ip_hdr, &saddr, &daddr);
        // printf("setup Done\n");
        // ip_hdr->daddr = recv_ip_hdr->saddr;
        
        // ip_hdr->daddr = recv_ip_hdr->saddr;
        // printf("recv_ip_hdr->saddr: %u\n", recv_ip_hdr->saddr);
        // printf("ip_hdr->daddr: %u\n", ip_hdr->daddr);
        // ip_hdr->check = findCheckSum((void *)ip_hdr,sizeof(struct iphdr));
        // memcpy(eth_hdr->h_dest,recv_eth_hdr->h_source,6);
        // memcpy(daddr.sll_addr,eth_hdr->h_dest,ETH_ALEN);
        ip_hdr->daddr = recv_ip_hdr->saddr;
        ip_hdr->check = findCheckSum((void *)ip_hdr,sizeof(struct iphdr));
        memcpy(eth_hdr->h_dest,recv_eth_hdr->h_source,6);
        memcpy(daddr.sll_addr,eth_hdr->h_dest,ETH_ALEN);
        // printf("here\n");

        if(sendto(sockfd, sendbuf, sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(response), 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_ll)) < 0){
            perror("sendto error");return 1;
        }
    }

    close(sockfd);
    return 0;
}

void setUpHeaders(struct ethhdr **eth_hdr, struct iphdr **ip_hdr, char *sendbuf, char *recvbuf, struct sockaddr_ll *saddr, struct sockaddr_ll *daddr, struct ifreq *index, struct ifreq *ip, struct ifreq *mac){
    memset(sendbuf, 0, BUF_SIZE);
    memset(recvbuf, 0, BUF_SIZE);

    *eth_hdr = (struct ethhdr *)sendbuf;
    (*eth_hdr)->h_proto = htons(ETH_P_IP);
    memcpy((*eth_hdr)->h_dest, mac->ifr_hwaddr.sa_data, ETH_ALEN);
    memcpy((*eth_hdr)->h_source, mac->ifr_hwaddr.sa_data, ETH_ALEN);

    // printf("etherned headers set\n");

    *ip_hdr = (struct iphdr *)(sendbuf + sizeof(struct ethhdr));
    (*ip_hdr)->ihl = 5;
    (*ip_hdr)->version = 4;
    (*ip_hdr)->tos = 16;
    (*ip_hdr)->tot_len = sizeof(struct iphdr) + sizeof(struct ethhdr) +sizeof(response);
    (*ip_hdr)->id = htons(10201);
    (*ip_hdr)->ttl = TTL;
    (*ip_hdr)->protocol = PROTOCOL;
    (*ip_hdr)->saddr = ((struct sockaddr_in *)&ip->ifr_addr)->sin_addr.s_addr;
    (*ip_hdr)->check = findCheckSum((void *)ip_hdr, sizeof(struct iphdr));

    // printf("Ip headers set\n");
}

void setUpResponse(response *res, query *qry, struct iphdr *recv_ip_hdr, struct ethhdr *recv_eth_hdr, struct ethhdr *eth_hdr, struct iphdr *ip_hdr, struct sockaddr_ll *saddr, struct sockaddr_ll *daddr){
    // printf("Entered\n");
    struct hostent *host;

    res->ID = qry->ID;
    res->type = 1;
    res->count = qry->count;
    printf("%d\n",qry->count);
    // printf("Halfway\n");
    for(int i=0;i<qry->count;i++){
        if(i==qry->count-1){
            qry->query[i][strlen(qry->query[i])-1]='\0';
        }
        host = gethostbyname(qry->query[i]);
        // printf("Query: %s %ld\n",qry->query[i],strlen(qry->query[i]));
        // printf("Host: %s\n",host->h_name);
        if(host==NULL){res->flag[i]=0;res->ipaddr[i]=0;}
        else{
            res->flag[i]=1;res->ipaddr[i]=htonl(((struct in_addr*)(host->h_addr))->s_addr);
        }
    }

    // printf("check\n");
    // printf("End\n");
}