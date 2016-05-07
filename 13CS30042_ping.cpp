// Assignment 6
// Implement ping using raw sockets

// Objective
// Implement a standard ping application which uses raw sockets.

// Group Details
// Member 1: Jeenu Grover (13CS30042)
// Member 2: Ashish Sharma (13CS30043)

// Filename: 13CS30042_ping.cpp

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
#include <signal.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <pthread.h>

using namespace std;

#define PACKETSIZE	128
typedef struct ICMP
{
	struct icmphdr ICMP_HDR;
	struct timeval tv;
	char payload[PACKETSIZE-sizeof(struct timeval)-sizeof(struct icmphdr)];
}ICMP;
vector<double> times;

struct timeval start;
string destHostName;

int send_cnt=0,recv_cnt=0;


// unsigned short in_cksum(unsigned short *ptr, int nbytes)
// {
//     register long sum;
//     u_short oddbyte;
//     register u_short answer;
 
//     sum = 0;
//     while (nbytes > 1) {
//         sum += *ptr++;
//         nbytes -= 2;
//     }
 
//     if (nbytes == 1) {
//         oddbyte = 0;
//         *((u_char *) & oddbyte) = *(u_char *) ptr;
//         sum += oddbyte;
//     }
 
//     sum = (sum >> 16) + (sum & 0xffff);
//     sum += (sum >> 16);
//     answer = ~sum;
 
//     return (answer);
// }

unsigned short checksum(void *b, int len)
{	unsigned short *buf = (unsigned short * )b;
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


void handle_receive(void *buf, int bytes)
{	int i;
	// struct iphdr *ip = (struct iphdr*)buf;
	// struct icmphdr *icmp = (struct icmphdr*)(buf+ip->ihl*4);
	char  * output = (char *)buf; 
	struct iphdr* recvIP = (struct iphdr  *)(output);
    struct icmphdr*  recvICMP = (struct icmphdr *)(output+sizeof(struct iphdr));

    struct timeval *tv = (struct timeval *)(output+sizeof(struct iphdr)+sizeof(struct icmphdr));

    long long int time_in_mill_1 = 1000000 * (tv->tv_sec) + (tv->tv_usec);

    struct timeval curr;
    gettimeofday(&curr,NULL);

    long long int time_in_mill_2 = 1000000 * (curr.tv_sec) + (curr.tv_usec);

    long long int latency = time_in_mill_2 - time_in_mill_1;

    double time_taken = (1.0*(latency/1000.0));
    if((recvICMP->un.echo.sequence<100))
	times.push_back(time_taken);    

    char * dest_IP = (char *)malloc(100*sizeof(char));

    inet_ntop(AF_INET, &(recvIP->saddr), dest_IP, INET_ADDRSTRLEN);

    struct timeval recvTime;

    gettimeofday(&recvTime,NULL);
    if((recvICMP->un.echo.sequence<100))
	cout<<bytes<<" bytes from "<<destHostName<<"("<<dest_IP<<"): icmp_seq = "<<(recvICMP->un.echo.sequence)<<" ttl = "<<(int)(recvIP->ttl)<<" time = "<<(1.0*(latency/1000.0))<<" ms"<<endl;
}

void * receive(void * args)
{
	int recvFD;
	struct sockaddr_in recvADDR;
	unsigned char output[1024];

	recvFD = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if ( recvFD < 0 )
	{
		perror("socket");
		exit(0);
	}
	while(1)
	{	int bytes, len=sizeof(recvADDR);

		memset(output,0, sizeof(output));
		bytes = recvfrom(recvFD, output, sizeof(output), 0, (struct sockaddr*)&recvADDR, (socklen_t*)&len);
		if ( bytes > 0 ){
			handle_receive(output, bytes);
			recv_cnt++;
			}
		else
			perror("recvfrom");
	}
}

pair<double,double> standard_deviation(vector<double> data)
{
	int n = data.size();
    double mean=0.0, sum_deviation=0.0;
    int i;
    for(i=0; i<n;++i)
    {
        mean+=data[i];
    }
    mean=mean/n;
    for(i=0; i<n;++i)
    sum_deviation+=(data[i]-mean)*(data[i]-mean);
    return pair<double,double>(mean,sqrt(sum_deviation/n));           
}

void handle(int param)
{
	struct timeval end;

	gettimeofday(&end,NULL);

	long long int time_in_mill_1 = 1000000 * (start.tv_sec) + (start.tv_usec);

	long long int time_in_mill_2 = 1000000 * (end.tv_sec) + (end.tv_usec);

	long long int latency = time_in_mill_2 - time_in_mill_1;

	cout<<"\n----- "<<destHostName<<" ping statistics -----"<<endl;
	cout<<send_cnt<<" packets transmitted, "<<recv_cnt<<" received "<<((send_cnt-recv_cnt)*100.0)/(send_cnt)<<"% packets loss, time "<<(1.0*(latency/1000.0))<<" ms"<<endl;
	pair<double,double> m_std = standard_deviation(times);

	double min_1 = *min_element(times.begin(),times.end());
	double max_1 = *max_element(times.begin(),times.end());

	cout<<"rtt min/avg/max/mdev "<<min_1<<"/"<<m_std.first<<"/"<<max_1<<"/"<<m_std.second<<endl;

	exit(1);
	
}

int main(int argc, char* argv[])
{

	if(argc<2)
    {
        cout<<argv[0]<<" Destination"<<endl;
        exit(1);
    }
    signal(SIGINT,handle);
    gettimeofday(&start,NULL);

	//struct sockaddr_in from,to;

	struct sockaddr_in destSaddr;

	destHostName = string(argv[1]);
    getIPAddr(destHostName,&destSaddr);
    destSaddr.sin_port = 0;
	char *destIP = inet_ntoa(destSaddr.sin_addr);

	

    int pid = getpid();

    pthread_t td;
    try{
    pthread_create(&td,NULL,&receive,NULL);

    }catch(const std::exception& e)
    {
        cout<<e.what()<<endl;
    } 

	ICMP icmp_packet;
	int i;
	int cnt = 1;
	int servFD = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    if(servFD<0)
    {
        perror("Socket() error");
        exit(-1);
    }

    int val = 255;

    if ( setsockopt(servFD, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
	perror("Set TTL option");

	cout<<"PING "<<destHostName<<"("<<destIP<<") 128 bytes of data"<<endl;

	while(1)
	{
		memset(&icmp_packet,0, sizeof(icmp_packet));
		icmp_packet.ICMP_HDR.type = ICMP_ECHO;
		icmp_packet.ICMP_HDR.un.echo.id = pid;
		gettimeofday(&icmp_packet.tv,NULL);
		strcpy(icmp_packet.payload,"PING.PAYLOAD");
		icmp_packet.ICMP_HDR.un.echo.sequence = cnt++;
		icmp_packet.ICMP_HDR.checksum = checksum(&icmp_packet, sizeof(icmp_packet));
		if ( sendto(servFD, &icmp_packet, sizeof(icmp_packet), 0, (struct sockaddr*)&destSaddr, sizeof(destSaddr)) <= 0 )
		perror("sendto");
		
		send_cnt++;	

		sleep(1);

	}
 	








	return 0;
}