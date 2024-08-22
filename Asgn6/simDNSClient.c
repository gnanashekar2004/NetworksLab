#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/time.h>

#define BUF_SIZE 1024
#define MAX_QUERIES 8
#define MAX_INPUT_SIZE 400
#define MAX_QUERY_SIZE 32
#define TIMEOUT 3 
#define PROTOCOL 254
#define TTL 64

#define INTERFACE "wlp2s0"
#define IP "127.0.0.1"
#define MAC "00:00:00:00:00:00"

typedef struct Table{
    uint16_t ID;
    int resent;
    char userInput[MAX_INPUT_SIZE];
    struct Table *next;
}Table;

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

int table_id = 0;

void fetchInterface(int sockfd, struct ifreq *if1, struct ifreq *if2, struct ifreq *if3){
    strncpy(if1->ifr_name, INTERFACE, IFNAMSIZ-1);
    strncpy(if2->ifr_name, INTERFACE, IFNAMSIZ-1);
    strncpy(if3->ifr_name, INTERFACE, IFNAMSIZ-1);

    if(ioctl(sockfd, SIOCGIFINDEX, if1) < 0){
        perror("SIOCGIFINDEX");exit(1);
    }
    if(ioctl(sockfd, SIOCGIFADDR, if2) < 0){
        perror("SIOCGIFADDR");exit(1);
    }
    if(ioctl(sockfd, SIOCGIFHWADDR, if3) < 0){
        perror("SIOCGIFHWADDR");exit(1);
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, INTERFACE, IFNAMSIZ-1) < 0){
        perror("SO_BINDTODEVICE");exit(1);
    }
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

void setUpHeaders(struct ethhdr **eth_hdr, struct iphdr **ip_hdr, char *sendbuf, char *recvbuf, struct sockaddr_ll *saddr, struct sockaddr_ll *daddr, struct ifreq *index, struct ifreq *ip, struct ifreq *mac){
    *eth_hdr = (struct ethhdr *)sendbuf;
    (*eth_hdr)->h_proto = htons(ETH_P_IP);
    memcpy((*eth_hdr)->h_source, mac->ifr_hwaddr.sa_data, 6);
    memset((*eth_hdr)->h_dest, 0xff, 6);
    // printf("check\n");
    sscanf(MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &(*eth_hdr)->h_dest[0], &(*eth_hdr)->h_dest[1], &(*eth_hdr)->h_dest[2], &(*eth_hdr)->h_dest[3], &(*eth_hdr)->h_dest[4], &(*eth_hdr)->h_dest[5]);
    // printf("check2\n");

    *ip_hdr = (struct iphdr *)(sendbuf + sizeof(struct ethhdr));
    (*ip_hdr)->ihl = 5;
    (*ip_hdr)->version = 4;
    (*ip_hdr)->tos = 16;
    (*ip_hdr)->id = htons(10201);
    (*ip_hdr)->ttl = TTL;
    (*ip_hdr)->protocol = PROTOCOL;
    (*ip_hdr)->saddr = ((struct sockaddr_in *)&ip->ifr_addr)->sin_addr.s_addr;
    (*ip_hdr)->daddr = inet_addr(IP);
    (*ip_hdr)->check = findCheckSum((uint16_t *)ip_hdr, sizeof(struct iphdr));

    // printf("end\n");
}

void manageTimeout(Table **head);
void manageInput(Table **head);
void manageResent(Table **head, Table **entry, Table **temp);
void manageSendQuery(int sockfd, char *sendbuf, Table *entry, struct sockaddr_ll *daddr);
void manageRecvResponse(response *res, Table **head);

int main(int argc, char *argv[]) {
    int sockfd;
    struct ifreq index;
    struct ifreq ip;
    struct ifreq mac;

    sockfd=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(sockfd<0){
        perror("socket creation error");return 1;
    }
    printf("socket created\n");

    //printf("Fetching interface\n");
    fetchInterface(sockfd, &index, &ip, &mac);
    // printf("Interface fetched\n");

    struct sockaddr_ll saddr;
    struct sockaddr_ll daddr;

    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = index.ifr_ifindex;
    daddr.sll_ifindex = index.ifr_ifindex;
    daddr.sll_halen = ETH_ALEN;
    if(bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0){
        perror("bind");exit(1);
    }
    printf("Binded\n");

    struct ethhdr *eth_hdr;
    struct iphdr *ip_hdr;
    char sendbuf[BUF_SIZE];
    char recvbuf[BUF_SIZE];

    setUpHeaders(&eth_hdr, &ip_hdr, sendbuf, recvbuf, &saddr, &daddr, &index, &ip, &mac);
    // printf("after setting header\n");
    memcpy(daddr.sll_addr, eth_hdr->h_dest, 6);

    query *qry;
    response *res;
    Table *head = NULL;
    Table *entry = NULL;
    Table *temp = NULL;

    qry = (query *)(sendbuf + sizeof(struct ethhdr) + sizeof(struct iphdr));

    // printf("before while\n");

    while(1){
        // printf("start\n");
        fd_set readfds;
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // printf("setup select\n");
        int result = select(sockfd+1, &readfds, NULL, NULL, &timeout);
        // printf("select done\n");

        if(result <0){
            perror("select");exit(1);
        }
        else if(result == 0){
            manageTimeout(&head);
        }
        else{
            if(FD_ISSET(sockfd,&readfds)){
                memset(recvbuf, 0, BUF_SIZE);

                int N = recvfrom(sockfd, recvbuf, BUF_SIZE, 0, NULL, NULL);
                if(N<0){
                    perror("recvfrom error");return 1;
                }

                struct ethhdr *recv_eth_hdr = (struct ethhdr *)recvbuf;
                struct iphdr *recv_ip_hdr = (struct iphdr *)(recvbuf + sizeof(struct ethhdr));
                
                if(recv_ip_hdr->protocol != PROTOCOL || ntohs(recv_eth_hdr->h_proto) != ETH_P_IP){
                    continue;
                }
                // printf("here\n");
                // printf("recv_ip_hdr->daddr: %u\n", recv_ip_hdr->daddr);
                // printf("ip_hdr->saddr: %u\n", ip_hdr->saddr);
                if(recv_ip_hdr->daddr != ip_hdr->saddr){
                    continue;
                }
                // printf("HERE\n");
                res = (response *)(recvbuf + sizeof(struct ethhdr) + sizeof(struct iphdr));
                if(res->type != 1){
                    continue;
                }

                // printf("something came\n");

                manageRecvResponse(res,&head);

                printf("Client received response\n");
            }
            if(FD_ISSET(STDIN_FILENO,&readfds)){
                // printf("something came\n");
                manageInput(&head);
            }

            entry = head;
            temp = head;

            // printf("CAME HERE\n");
            while(entry != NULL ){
                // printf("came here\n");
                if(entry->resent >= 3){
                    manageResent(&head,&entry,&temp);
                }else{
                    // printf("sending query\n");
                    manageSendQuery(sockfd,sendbuf,entry,&daddr);
                    printf("Query %d send\n",entry->ID);
                    entry = entry->next;
                }
            }
        }
    }

    close(sockfd);
    return 0;
}

void manageTimeout(Table **head){
    Table *entry = *head;

    while(entry != NULL){
        entry->resent++;
        entry = entry->next;
    }
}

void manageInput(Table **head){
    char input[MAX_INPUT_SIZE];
    Table *newEntry = (Table *)malloc(sizeof(Table));
    Table *entry;

    fgets(input, MAX_INPUT_SIZE, stdin);
    if(strncmp(input, "EXIT", 4) == 0){
        free(newEntry);exit(0);
    }
    // printf("Query: %s\n", input);

    newEntry->ID = (table_id++)%65536;
    newEntry->resent = 0;
    strncpy(newEntry->userInput, input, strlen(input));
    newEntry->userInput[strlen(input)] = '\0';
    newEntry->next = NULL;

    if(*head == NULL){
        *head = newEntry;
    }else{
        entry = *head;
        while(entry->next != NULL){
            entry = entry->next;
        }
        entry->next = newEntry;
    }
    printf("end\n");
}

void manageResent(Table **head, Table **entry, Table **temp){
    // printf("Resent %d times\n", entry->ID);
    printf("deleting query due to exceeding max resent limit\n");
    if(*entry == *head){
        *head = (*head)->next;
        free(*entry);
        *entry = *head;
        *temp = *head;
    }else{
        *temp= (*entry)->next;
        free(*entry);
        *entry = *temp;
    }
}

void manageSendQuery(int sockfd, char *sendbuf, Table *entry, struct sockaddr_ll *daddr){
    char *token;
    char temp[MAX_INPUT_SIZE];
    
    query *qry = (query *)(sendbuf + sizeof(struct ethhdr) + sizeof(struct iphdr));
    qry->ID = entry->ID;
    qry->type = 0;
    
    strncpy(temp, entry->userInput, strlen(entry->userInput));
    temp[strlen(entry->userInput)] = '\0';

    token = strtok(temp," ");
    token = strtok(NULL," ");
    qry->count = atoi(token);
    for(int i=0; i<=qry->count; i++){
        token = strtok(NULL," ");
        if(token == NULL){
            break;
        }
        size_t len = strlen(token);
        if(len>=sizeof(qry->query[i])){
            len = sizeof(qry->query[i])-1;
        }
        strncpy(qry->query[i], token, len);
        qry->query[i][len] = '\0';
        qry->size[i] = len;
    }
    // printf("type: %d\n",qry->type);
    int n = sendto(sockfd, sendbuf,BUF_SIZE, 0, (struct sockaddr *)daddr, sizeof(struct sockaddr_ll));
    if(n<0){
        perror("sendto error");exit(1);
    }
}

void manageRecvResponse(response *res, Table **head){
    Table *entry = *head;
    Table *temp = NULL;
    char *token;

    printf("Receiving msg\n");

    while(entry!=NULL){
        if(entry->ID == res->ID){
            printf("\t\t\t\tQuery ID: %d\n", res->ID);
            printf("\t\t\t\tTotal query strings: %d\n", res->count);

            token = strtok(entry->userInput, " ");
            token = strtok(NULL, " ");
            int n=atoi(token);

            // for(int i=0;i<n;i++){
            //     printf("%d\n",res->flag[i]);
            // }
            for(int i=0;i<n;i++){
                token = strtok(NULL," ");
                if(token == NULL){
                    break;
                }
                if(i==n-1){
                    token[strlen(token)-1] = '\0';
                }
                printf("\t\t\t\t%s ",token);
                if(res->flag[i] == 0){
                    printf("NO IP ADDRESS FOUND\n");
                }else{
                    printf("%s\n",inet_ntoa(*(struct in_addr *)&res->ipaddr[i]));
                }
            }
            if(temp==NULL){
                *head = entry->next;
            }else{
                temp->next = entry->next;
            }
            Table *nxt = entry->next;
            free(entry);
            entry = nxt;
        }else{
            temp = entry;
            entry = entry->next;
        }
    }
}