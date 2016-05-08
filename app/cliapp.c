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
    int request_id;
    struct proposer_state *proposer;
    struct timespec *starts;
    struct request req;
};

void run_test(struct app_ctx *state);

struct app_ctx *new_app_ctx() {
    struct app_ctx *state = malloc(sizeof(struct app_ctx));
    state->request_id = 0;
    char key[] = "abcde";
    char value[] = "123456";
    memcpy(state->req.key, key, 5);
    memcpy(state->req.value, value, 6);
    return state;
}

void free_app_ctx(struct app_ctx *state) {
    free_proposer(state->proposer);
    free(state);
}


int deliver_response(char* res, int rsize, void* arg_ctx) {
    struct app_ctx *state = arg_ctx;
    struct request *req = (struct request *)res;
    if (state->proposer->conf->verbose) {
        printf("on application %d\n", req->request_id);
    }
    state->request_id = req->request_id;
    run_test(state);
    return 0;
}


void run_test(struct app_ctx *state) {
    state->req.request_id = state->request_id++;
    state->req.op = (random() % 2) ? PUT : GET;
    int size = sizeof(struct request);
    submit(state->proposer, (char *)&state->req, size);
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