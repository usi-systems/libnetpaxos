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
        m->msgtype, m->inst, m->rnd, m->vrnd, m->acptid, m->paxosval);
}

void pack(Message *dst, const Message *src) {
    dst->inst       = htonl(src->inst);
    dst->rnd        = htons(src->rnd);
    dst->vrnd       = htons(src->vrnd);
    dst->acptid     = htons(src->acptid);
    dst->msgtype    = htons(src->msgtype);
    strcpy(dst->paxosval,   src->paxosval);
    dst->client     = src->client;
}

void unpack(Message *dst, const Message *src) {
    dst->inst         = ntohl(src->inst);
    dst->rnd          = ntohs(src->rnd);
    dst->vrnd         = ntohs(src->vrnd);
    dst->acptid       = ntohs(src->acptid);
    dst->msgtype      = ntohs(src->msgtype);
    strcpy(dst->paxosval,   src->paxosval);
    dst->client     = src->client;
}

void initialize_message(Message *m, int msgtype) {
    m->inst     = 0;
    m->rnd      = 0;
    m->vrnd     = 0;
    m->acptid    = 0;
    m->msgtype  = msgtype;
    bzero(m->paxosval, PAXOS_VALUE_SIZE);
}
