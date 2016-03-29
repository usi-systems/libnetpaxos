#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "message.h"


void message_to_string(Message m, char *str) {
    sprintf(str,
        "msgtype:  %.4x\n"
        "instance: %.8x\n"
        "round:    %.4x\n"
        "vround:   %.4x\n"
        "acceptor: %.4x\n"
        "time:     %lld.%06ld\n"
        "value:    %.8x\n"
        "sta_high: %.8x\n"
        "sta_low:  %.8x\n"
        "end_high: %.8x\n"
        "end_low:  %.8x\n",
        m.mstype, m.inst, m.rnd, m.vrnd,
        m.acpid,
        (long long)m.ts.tv_sec, m.ts.tv_nsec,
        m.value,
        m.csh, m.csl,
        m.ceh, m.cel );
}

size_t pack(const Message *msg, char *buf) {
    Message s;
    s.inst = htonl(msg->inst);
    s.rnd = htons(msg->rnd);
    s.vrnd = htons(msg->vrnd);
    s.acpid = htons(msg->acpid);
    s.mstype = htons(msg->mstype);
    s.value = htonl(msg->value);
    s.csh = htonl(msg->csh);
    s.csl = htonl(msg->csl);
    s.ceh = htonl(msg->ceh);
    s.cel = htonl(msg->cel);
    s.ash = htonl(msg->ash);
    s.asl = htonl(msg->asl);
    s.aeh = htonl(msg->aeh);
    s.ael = htonl(msg->ael);
    s.ts = msg->ts;
    memcpy(buf, &s, sizeof(s));
    return sizeof(s);
}

void unpack(Message *m) {
    m->inst = ntohl(m->inst);
    m->rnd = ntohs(m->rnd);
    m->vrnd = ntohs(m->vrnd);
    m->acpid = ntohs(m->acpid);
    m->mstype = ntohs(m->mstype);
    m->value = ntohl(m->value);
    m->csh = ntohl(m->csh);
    m->csl = ntohl(m->csl);
    m->ceh = ntohl(m->ceh);
    m->cel = ntohl(m->cel);
    m->ash = ntohl(m->ash);
    m->asl = ntohl(m->asl);
    m->aeh = ntohl(m->aeh);
    m->ael = ntohl(m->ael);
}
