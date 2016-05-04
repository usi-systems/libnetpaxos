#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "proposer.h"
#include "application.h"
#include "netpaxos_utils.h"

struct app_ctx {
    int mps;
    char* buffer;
    struct proposer_state *proposer;
};

void run_test(struct app_ctx *state);

struct app_ctx *new_app_ctx() {
    struct app_ctx *state = malloc(sizeof(struct app_ctx));
    state->mps = 0;
    state->buffer = malloc(64);
    bzero(state->buffer, 64);
    return state;
}

void free_app_ctx(struct app_ctx *state) {
    free_proposer(state->proposer);
    free(state->buffer);
    free(state);
}


int deliver_response(char* res, int rsize, void* arg_ctx) {
    struct app_ctx *state = arg_ctx;
    state->mps++;
    if (state->proposer->conf->verbose) {
        int *port = (int *)res;
        printf("on application %d\n", *port);
    }
    run_test(state);
    return 0;
}

int craft_message(char** buffer) {
    char key[] = "abcde123456789";
    char value[] = "zxcvbnmasdfghj";

    (*buffer)[0] = (random() % 2) ? PUT : GET;
    char ksize = (unsigned char) strlen(key);
    char vsize = (unsigned char) strlen(value);
    (*buffer)[1] = ksize;
    (*buffer)[2] = vsize;
    memcpy(&(*buffer)[3], key, ksize);
    memcpy(&(*buffer)[ 3 + ksize ], value, vsize);
    int size = ksize + vsize + 4; // 3 for three chars and 1 for terminator
    return size;
}

void run_test(struct app_ctx *state) {
    int size = craft_message(&state->buffer);
    submit(state->proposer, state->buffer, size);
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("%s config eth outstanding\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct app_ctx *state = new_app_ctx();
    state->proposer = make_proposer(argv[1], argv[2]);
    set_application_ctx(state->proposer, state);
    register_callback(state->proposer, deliver_response);
    int npackets = atoi(argv[3]);
    int i;
    for (i = 0; i < npackets; i++) {
        run_test(state);
    }
    event_base_dispatch(state->proposer->base);
    free_app_ctx(state);
    return (EXIT_SUCCESS);
}