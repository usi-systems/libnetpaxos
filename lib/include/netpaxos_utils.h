#ifndef _NETPAXOS_UTILS_H_
#define _NETPAXOS_UTILS_H_

#include <time.h>
#include <stdint.h>

void gettime(struct timespec * ts);
int timediff(struct timespec *result, struct timespec *end, struct timespec *start);
int compare_ts(struct timespec *time1, struct timespec *time2);
int timeval_subtract (struct timeval *result, struct timeval *start, struct timeval *end);
int create_server_socket(int port);
void addMembership(char *group, int sockfd);
void setReuseAddr(int sockfd);
void setRcvBuf(int sockfd);
void setReusePort(int sockfd);
#endif