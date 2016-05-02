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
    struct timespec start;
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
    // printf("on application %s\n", res);
    struct timespec result, end;
    gettime(&end);
    int negative = timediff(&result, &end, &state->start);
    if (negative) {
        fprintf(stderr, "Latency is negative\n");
    } else {
        double latency = (result.tv_sec + ((double)result.tv_nsec) / 1e9);
        fprintf(stdout, "%.9f\n", latency);
    }

    run_test(state);
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

void run_test(struct app_ctx *state) {
    int size = craft_message(&state->buffer);
    submit(state->proposer, state->buffer, size);
    gettime(&state->start);
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config eth\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct app_ctx *state = new_app_ctx();
    state->proposer = make_proposer(argv[1], argv[2]);
    set_application_ctx(state->proposer, state);
    register_callback(state->proposer, deliver_response);
    run_test(state);
    event_base_dispatch(state->proposer->base);
    free_app_ctx(state);
    return (EXIT_SUCCESS);
}