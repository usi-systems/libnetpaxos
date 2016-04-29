#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "proposer.h"
#include "application.h"

struct app_ctx {
    int mps;
};

int deliver_response(char* res, int rsize, void* arg_ctx) {
    struct app_ctx *state = arg_ctx;
    state->mps++;
    printf("on application %s\n", res);
    return 0;
}

int craft_message(char** buffer) {
    char key[] = "abcde123456789";
    char value[] = "zxcvbnmasdfghj";
    (*buffer)[0] = GET;
    char ksize = (unsigned char) strlen(key);
    char vsize = (unsigned char) strlen(value);
    (*buffer)[1] = ksize;
    (*buffer)[2] = vsize;
    memcpy(&(*buffer)[3], key, ksize);
    memcpy(&(*buffer)[ 3 + ksize ], value, vsize);
    int size = ksize + vsize + 4; // 3 for three chars and 1 for terminator
    return size;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config eth\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct app_ctx state;
    struct proposer_state *ctx = make_proposer(argv[1], argv[2]);
    char *buffer = malloc(64);
    int size = craft_message(&buffer);
    submit(buffer, size, ctx, deliver_response, &state);
    free_proposer(ctx);
    return (EXIT_SUCCESS);
}