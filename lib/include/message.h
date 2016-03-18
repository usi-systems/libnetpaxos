#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <stdint.h>
#include <time.h>

typedef struct Message {
    uint32_t inst;
    uint16_t rnd;
    uint16_t vrnd;
    uint16_t acpid;
    uint16_t mstype;
    uint32_t value;
    struct timespec ts;
    char   junk[1400];
} Message;

void message_to_string(Message m, char *str);
size_t pack(const Message *msg, char *buf);
void unpack(Message *m);

#endif