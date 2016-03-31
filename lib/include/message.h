#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <stdint.h>
#include <time.h>

typedef struct Message {
    uint32_t inst;
    uint16_t rnd;
    uint16_t vrnd;
    uint16_t acpid;
    uint16_t msgtype;
    uint32_t value;
    uint32_t fsh;
    uint32_t fsl;
    uint32_t feh;
    uint32_t fel;
    uint32_t csh;
    uint32_t csl;
    uint32_t ceh;
    uint32_t cel;
    uint32_t ash;
    uint32_t asl;
    uint32_t aeh;
    uint32_t ael;
    struct timespec ts;
    uint32_t psize;
} Message;

typedef struct TimespecMessage {
    struct timespec ts;
    char   junk[sizeof(Message) - sizeof(struct timespec)];
} TimespecMessage;


void print_message(Message m);
size_t pack(const Message *msg, char *buf);
void unpack(Message *m);
void initialize_message(Message *m, int msgtype, int val, int padsize);
#endif