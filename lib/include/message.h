#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <stdint.h>
#include <time.h>

#define PAXOS_VALUE_SIZE 32

typedef struct Message {
    uint32_t inst;
    uint16_t rnd;
    uint16_t vrnd;
    uint16_t acptid;
    uint16_t msgtype;
    // Fixed value size: 32 Bytes
    char paxosval[PAXOS_VALUE_SIZE];
    struct sockaddr_in client;
} Message;


void print_message(Message *m);
void pack(Message *dst, const Message *src);
void unpack(Message *dst, const Message *src);
void initialize_message(Message *m, int msgtype);
#endif