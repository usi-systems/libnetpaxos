#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "stdint.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "netpaxos_utils.h"
#include <arpa/inet.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

void gettime(struct timespec * ts) {
#ifdef __MACH__ 
        clock_serv_t cclock;
        mach_timespec_t mts;
        host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
        clock_get_time(cclock, &mts);
        mach_port_deallocate(mach_task_self(), cclock);
        ts->tv_sec = mts.tv_sec;
        ts->tv_nsec = mts.tv_nsec;

#else
        clock_gettime(CLOCK_REALTIME, ts);
#endif
}

int timediff(struct timespec *result, struct timespec *end, struct timespec *start)
{
  result->tv_sec = end->tv_sec - start->tv_sec;
  result->tv_nsec = end->tv_nsec - start->tv_nsec;

  /* Return 1 if result is negative. */
  return end->tv_sec < start->tv_sec;
}


int compare_ts(struct timespec *time1, struct timespec *time2) {
        if (time1->tv_sec < time2->tv_sec)
            return (-1) ;               /* Less than */
        else if (time1->tv_sec > time2->tv_sec)
            return (1) ;                /* Greater than */
        else if (time1->tv_nsec < time2->tv_nsec)
            return (-1) ;               /* Less than */
        else if (time1->tv_nsec > time2->tv_nsec)
            return (1) ;                /* Greater than */
        else
            return (0) ;                /* Equal */
}


/*
    Generic checksum calculation function
*/
unsigned short csum(unsigned short *ptr, int nbytes)
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }

    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;
    return(answer);
}


int create_rawsock() {
    int s = socket (AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(s == -1)
    {
        perror("Failed to create raw socket");
        exit(1);
    }
    return s;
}

int create_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}


int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }
    setReuseAddr(sockfd);
    setRcvBuf(sockfd);
    setReusePort(sockfd);
    printf("Listening on port %d.\n", port);
    return sockfd;
}

void addMembership(char *group, int sockfd) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq))<0) {
        perror("IP_ADD_MEMBERSHIP");
        exit(EXIT_FAILURE);
    }
}


void setReuseAddr(int sockfd) {
    int yes = 1;
    if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
    {
        perror("set SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }
}

void setReusePort(int sockfd) {
    int yes = 1;
    if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) == -1 )
    {
        perror("set SO_REUSEPORT");
        exit(EXIT_FAILURE);
    }
}

void setRcvBuf(int sockfd) {
    int rcvbuf = 16777216;
    socklen_t size = sizeof(sockfd);
    if ( setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbuf, size) == -1 )
    {
        perror("set SO_RCVBUF");
        exit(EXIT_FAILURE);
    }
}

void send_msg(int sock, char* msg, int size, struct sockaddr_in *remote) {
    // printf("sock %d\n", sock);
    // printf("Msg %s\n", msg);
    // printf("size %d\n", size);
    // printf("address %s, port %d\n", inet_ntoa(remote->sin_addr), ntohs(remote->sin_port));
    int n = sendto(sock, msg, size, 0, (struct sockaddr*) remote, sizeof(*remote));
    if (n < 0) {
        perror("ERROR in sendto");
        return;
    }
}
