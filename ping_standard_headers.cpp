#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <iomanip>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/time.h>

#define MAX_PACKET_LEN 8192
#define MAX_DATA_LEN 8100
#define PACKET_LEN 60
#define TIMEVAL 3

#define PAYLOADSIZE 16

#define ICMP struct icmphdr
#define IP struct iphdr

using namespace std;

int ID = 1;

long long int time_in_mill_1;
long long int time_in_mill_2;
long long int latency_1;

unsigned short in_cksum(unsigned short *ptr, int nbytes)
{
    register long sum;
    u_short oddbyte;
    register u_short answer;
 
    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
 
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char *) & oddbyte) = *(u_char *) ptr;
        sum += oddbyte;
    }
 
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
 
    return (answer);
}

unsigned short compute_icmp_checksum(void *b, int len)
{   
    unsigned short *buf = (unsigned short *)b;
    unsigned int sum=0;
    unsigned short result;

    for ( sum = 0; len > 1; len -= 2 )
        sum += *buf++;
    if ( len == 1 )
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void getIPAddr(string hostName,sockaddr_in *saddr)
{
    unsigned int address;
    hostent *he;
    struct in_addr **addr_list;

    memset(saddr,0,sizeof(*saddr));

    address = inet_addr(&hostName[0]);

    if(address == INADDR_NONE)
    {
        he = gethostbyname(&hostName[0]);
        if(he == 0)
        {
            cout<<"Could Not Resolve Host "<<hostName<<endl;
            exit(1);
        }
        else
        {
            saddr->sin_family = AF_INET;
            addr_list = (struct in_addr **) he->h_addr_list;
     
            for(int i = 0; addr_list[i] != NULL; i++) 
            {
                //Return the first one;
                memcpy((void *)&(saddr->sin_addr), he->h_addr_list[i], he->h_length);
                //(saddr->sin_addr) = *addr_list[i];

                //cout<<"destIP: "<<inet_ntoa(*addr_list[i])<<endl;
                break;
            }
            //memset(&(saddr->sin_addr), (char *)hp->h_addr, hp->h_length);
        }
    }

    else
    {
        (saddr)->sin_family = AF_INET;
        (saddr->sin_addr).s_addr = address;
    }
}

int main(int argc,char *argv[])
{
    if(argc<3)
    {
        cout<<argv[0]<<" SourceIP Destination"<<endl;
        exit(1);
    }

    int servFD,i=1,ttl = 1,temp = 1,cnt = 13;
    sockaddr_in srcSaddr,destSaddr;
    char *output,*dest_IP;
    string destHostName(argv[2]);

    // Get Destination IP
    getIPAddr(destHostName,&destSaddr);

    char *destIP = inet_ntoa(destSaddr.sin_addr);

    destSaddr.sin_port = 10002;

    servFD = socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(servFD<0)
    {
        perror("Socket() error");
        exit(-1);
    }

    if(setsockopt(servFD,IPPROTO_IP,IP_HDRINCL,&temp,sizeof(temp)) < 0)
    {
        perror("setsockopt() error");
        exit(-1);
    }

    //allow socket to send datagrams to broadcast addresses
    if (setsockopt (servFD, SOL_SOCKET, SO_BROADCAST, (const char*)&temp, sizeof (temp)) == -1) 
    {
        perror("setsockopt");
        return (0);
    }   

    cout<<"PING "<<destHostName<<"("<<destIP<<") 56(84) bytes of data"<<endl;

    char payload[PAYLOADSIZE];
    strcpy(payload,"PING");

    char *icmp_packet = (char *)malloc(sizeof(IP)+sizeof(ICMP)+sizeof(struct timeval)+strlen(payload));

    IP *recvIP;
    ICMP *recvICMP;

    struct iphdr *sendIP = (struct iphdr *)(icmp_packet);
    struct icmphdr *sendICMP = (struct icmphdr *)(icmp_packet+sizeof(IP));
    struct timeval *tp =(struct timeval *)(icmp_packet+sizeof(IP)+sizeof(ICMP)) ;
    memset(icmp_packet,0,sizeof(IP)+sizeof(ICMP)+sizeof(struct timeval)+strlen(payload));

    memcpy(icmp_packet+sizeof(IP)+sizeof(ICMP)+sizeof(struct timeval),payload,strlen(payload));



    sendIP->version = 4;
    sendIP->tos = 16;
    sendIP->frag_off = 0;
    sendIP->tot_len = htons(sizeof(IP)+sizeof(ICMP)+sizeof(struct timeval)+strlen(payload));
    sendIP->ttl = 255;
    sendIP->ihl = 5;
    sendIP->protocol = IPPROTO_ICMP;
    sendIP->saddr = inet_addr(argv[1]);
    sendIP->daddr = inet_addr(destIP);
 
    sendICMP->type = ICMP_ECHO;
    sendICMP->code = 0;

    fd_set active_sock_fd_set, read_fd_set;

    struct  timeval timeout;
    timeout.tv_sec = TIMEVAL;
    timeout.tv_usec = 0;

    for(i=1;i<=cnt;++i)
    {
        FD_ZERO(&active_sock_fd_set);
        FD_SET(servFD, &active_sock_fd_set);

        read_fd_set = active_sock_fd_set;

        timeout.tv_sec = TIMEVAL;
        timeout.tv_usec = 0;

        sendICMP->un.echo.id= i;
        sendICMP->un.echo.sequence = i;
        //checksum
        sendICMP->checksum = 0;
        sendICMP->checksum = in_cksum((unsigned short *)sendICMP,sizeof(ICMP)+sizeof(struct timeval)+strlen(payload));

        gettimeofday(tp,NULL);

        if(sendto(servFD,icmp_packet, sizeof(IP)+sizeof(ICMP)+sizeof(struct timeval)+strlen(payload), 0, (struct sockaddr *)&destSaddr, sizeof(destSaddr)) < 0)
        {
            perror("sendto() error");
            exit(-1);
        }

        if(select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout) <= 0)
        {
            cout<<"Timeout"<<endl;
        }

        else{
            if(FD_ISSET(servFD, &read_fd_set)){

                output = (char *)malloc(MAX_PACKET_LEN*sizeof(char));

                struct sockaddr_in saddr;
                int saddr_len = sizeof(saddr);
                int buflen = recvfrom(servFD,output,MAX_PACKET_LEN,0,(struct sockaddr *)(&saddr),(socklen_t *)&saddr_len);

                if(buflen<0)
                {
                    printf("error in reading recvfrom function\n");
                    return 0;
                }

                // decode Recieved Packet

                recvIP = (IP *)(output);
                recvICMP = (ICMP *)(output+sizeof(IP));
                struct timeval *sendTime = (struct timeval *)(output+sizeof(IP)+sizeof(ICMP));

                time_in_mill_1 = 1000000 * (sendTime->tv_sec) + (sendTime->tv_usec);

                dest_IP = (char *)malloc(100*sizeof(char));

                inet_ntop(AF_INET, &(recvIP->saddr), dest_IP, INET_ADDRSTRLEN);

                struct timeval recvTime;

                gettimeofday(&recvTime,NULL);

                time_in_mill_2 = 1000000 * (recvTime.tv_sec) + (recvTime.tv_usec);
                latency_1 = time_in_mill_2 - time_in_mill_1;

                cout<<buflen<<" bytes from ("<<dest_IP<<"): icmp_seq="<<(recvICMP->un.echo.sequence)<<" ttl = "<<(recvIP->ttl)<<" time="<<(1.0*(latency_1/1000.0))<<" ms"<<endl;
            }
        }

    }
    return 0;
}