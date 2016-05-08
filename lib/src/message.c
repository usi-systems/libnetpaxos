#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "message.h"


void print_message(Message *m) {
    fprintf(stdout,
        "msgtype:  %d\t"
        "instance: %d\t"
        "round:    %d\t"
        "vround:   %d\n"
        "acceptor: %d\t"
        "paxosval: %s\n",
        m->msgtype, m->inst, m->rnd, m->vrnd, m->acptid, m->paxosval + 5);
}

void pack(Message *src) {
    src->inst       = htonl(src->inst);
    src->rnd        = htons(src->rnd);
    src->vrnd       = htons(src->vrnd);
    src->acptid     = htons(src->acptid);
    src->msgtype    = htons(src->msgtype);
    src->client     = src->client;
}

void unpack(Message *src) {
    src->inst         = ntohl(src->inst);
    src->rnd          = ntohs(src->rnd);
    src->vrnd         = ntohs(src->vrnd);
    src->acptid       = ntohs(src->acptid);
    src->msgtype      = ntohs(src->msgtype);
    src->client       = src->client;
}

void initialize_message(Message *m, int msgtype) {
    m->inst     = 0;
    m->rnd      = 0;
    m->vrnd     = 0;
    m->acptid    = 0;
    m->msgtype  = msgtype;
    bzero(m->paxosval, PAXOS_VALUE_SIZE);
}
