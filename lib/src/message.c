#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "message.h"


void print_message(Message m) {
    fprintf(stdout,
        "msgtype:  %d\t"
        "instance: %d\t"
        "round:    %d\t"
        "vround:   %d\n"
        "acceptor: %d\t"
        "value:    %d\n"
        "psize:    %d\n"
        "fsh:      %d\t"
        "fsl:      %d\t"
        "feh:      %d\t"
        "fel:      %d\n"
        "csh:      %d\t"
        "csl:      %d\t"
        "ceh:      %d\t"
        "cel:      %d\n"
        "ash:      %d\t"
        "asl:      %d\t"
        "aeh:      %d\t"
        "ael:      %d\n",
        m.msgtype, m.inst, m.rnd, m.vrnd, m.acpid, m.value, m.psize,
        m.fsh, m.fsl, m.feh, m.fel,
        m.csh, m.csl, m.ceh, m.cel,
        m.ash, m.asl, m.aeh, m.ael);
}

size_t pack(const Message *msg, char *buf) {
    Message s;
    s.inst = htonl(msg->inst);
    s.rnd = htons(msg->rnd);
    s.vrnd = htons(msg->vrnd);
    s.acpid = htons(msg->acpid);
    s.msgtype = htons(msg->msgtype);
    s.value = htonl(msg->value);
    s.psize = htonl(msg->psize);
    s.fsh = htonl(msg->fsh);
    s.fsl = htonl(msg->fsl);
    s.feh = htonl(msg->feh);
    s.fel = htonl(msg->fel);
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
    m->msgtype = ntohs(m->msgtype);
    m->value = ntohl(m->value);
    m->psize = ntohl(m->psize);
    m->fsh = ntohl(m->fsh);
    m->fsl = ntohl(m->fsl);
    m->feh = ntohl(m->feh);
    m->fel = ntohl(m->fel);
    m->csh = ntohl(m->csh);
    m->csl = ntohl(m->csl);
    m->ceh = ntohl(m->ceh);
    m->cel = ntohl(m->cel);
    m->ash = ntohl(m->ash);
    m->asl = ntohl(m->asl);
    m->aeh = ntohl(m->aeh);
    m->ael = ntohl(m->ael);
}

void initialize_message(Message *m, int msgtype, int val, int padsize) {
    m->inst     = 0;
    m->rnd      = 0;
    m->vrnd     = 0;
    m->acpid    = 0;
    m->msgtype  = msgtype;
    m->value    = val;
    m->psize    = padsize;
    m->fsh      = 0;
    m->fsl      = 0;
    m->feh      = 0;
    m->fel      = 0;
    m->csh      = 0;
    m->csl      = 0;
    m->ceh      = 0;
    m->cel      = 0;
    m->ash      = 0;
    m->asl      = 0;
    m->aeh      = 0;
    m->ael      = 0;
}
