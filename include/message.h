#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <stdint.h>
#include <sys/time.h>

typedef struct Message {
    uint32_t inst;
    uint16_t rnd;
    uint16_t vrnd;
    uint32_t acpid;
    uint16_t mstype;
    uint16_t valsize;
    uint32_t value;
    struct timeval ts;
} Message;

void message_to_string(Message m, char *str);
size_t pack(const Message *msg, char *buf);
void unpack(Message *m);

#endif