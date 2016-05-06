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
    int req_id;
    char* buffer;
    struct proposer_state *proposer;
    struct timespec *starts;
};

void run_test(struct app_ctx *state);

struct app_ctx *new_app_ctx() {
    struct app_ctx *state = malloc(sizeof(struct app_ctx));
    state->req_id = 0;
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
    int *req_id = (int *)res;
    if (state->proposer->conf->verbose) {
        printf("on application %d\n", *req_id);
    }
    state->req_id = *req_id;
    run_test(state);
    return 0;
}

/* Key value store client */

int craft_message(char** buffer, int req_id) {
    memcpy(*buffer, &req_id, sizeof(int));
    int intsize = sizeof(int);
    char key[] = "abcde12345";
    char value[] = "zxcvbnmasdfghj";
    (*buffer)[intsize] = (random() % 2) ? PUT : GET;
    char ksize = (unsigned char) strlen(key);
    char vsize = (unsigned char) strlen(value);
    (*buffer)[intsize + 1] = ksize;
    (*buffer)[intsize + 2] = vsize;
    memcpy(&(*buffer)[intsize + 3], key, ksize);
    memcpy(&(*buffer)[ intsize + 3 + ksize ], value, vsize);
    int size = intsize + 3 + ksize + vsize;
    return size;
}


void run_test(struct app_ctx *state) {
    int size = craft_message(&state->buffer, state->req_id);
    submit(state->proposer, state->buffer, size);
    // submit(state->proposer, (char*)&state->req_id, sizeof(state->req_id));
    state->req_id++;
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("%s config eth outstanding\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int npackets = atoi(argv[3]);
    struct app_ctx *state = new_app_ctx();
    state->proposer = make_proposer(argv[1], argv[2], npackets);
    set_application_ctx(state->proposer, state);
    register_callback(state->proposer, deliver_response);
    int i;
    for (i = 0; i < npackets; i++) {
        run_test(state);
    }
    event_base_dispatch(state->proposer->base);
    free_app_ctx(state);
    return (EXIT_SUCCESS);
}