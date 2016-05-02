#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "uthash.h"
#include <stdio.h>
#include <unistd.h>
/* inet_ntoa */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* end inet_ntoa */
#include "application.h"
#include "learner.h"
#include "config.h"
#include "netpaxos_utils.h"

struct kv_entry {
    const char *key;                    /* key */
    char value[10];
    UT_hash_handle hh;         /* makes this structure hashable */
};

struct application {
    int sock;
    struct kv_entry *hashmap;
} application;

struct application* application_new() {
    struct application *state = malloc(sizeof(struct application));
    state->hashmap = NULL;    /* important! initialize to NULL */
    state->sock = create_socket();
    return state;
}

void application_destroy(struct application *state) {
    free(state);
}

void add_entry(struct kv_entry **hashmap, const char *key, int ksize, const char *value, int vsize) {
    struct kv_entry *s;
    HASH_FIND_STR(*hashmap, key, s);
    if (s==NULL) {
        s = malloc(sizeof(struct kv_entry));
        s->key = key;
        HASH_ADD_KEYPTR( hh, *hashmap, s->key, ksize, s );  /* id: value of key field */
    }
    memcpy(s->value, value, vsize);
}


struct kv_entry *find_entry(struct kv_entry **hashmap, const char *key, int ksize) {
    struct kv_entry *s;
    HASH_FIND_STR( *hashmap, key, s );  /* s: output pointer */
    return s;
}


void delete_entry(struct kv_entry **hashmap, struct kv_entry *entry) {
    HASH_DEL( *hashmap, entry);  /* entry: pointer to deletee */
    free(entry);              /* optional; it's up to you! */
}


int deliver(struct LearnerCtx *ctx, int inst, char* value, int size) {
    struct app_request *req = (struct app_request *) value;
    struct application *state = ctx->app;
    char *request = req->value;

    if (!request || request[0] == '\0') {
        return FAILED;
    }
    char op = request[0];

    switch(op) {
        case PUT: {
            unsigned char ksize = request[1];
            unsigned char vsize = request[2];
            add_entry(&state->hashmap, &request[3], ksize, &request[3+ksize], vsize);
            int res = SUCCESS;
            send_msg(state->sock, (char*)&res, 1, req->client);
            return SUCCESS;
        }
        case GET: {
            unsigned char ksize = request[1];
            struct kv_entry *entry = find_entry(&state->hashmap, &request[3], ksize);
            if (entry) {
                int vsize = strlen(entry->value);
                send_msg(state->sock, entry->value, vsize, req->client);
                return GOT_VALUE;
            }
            int res = NOT_FOUND;
            send_msg(state->sock, (char*)&res, 1, req->client);
            return NOT_FOUND;
        }
        case DELETE: {
            unsigned char ksize = request[1];
            struct kv_entry *entry = find_entry(&state->hashmap, &request[3], ksize);
            if (entry) {
                delete_entry(&state->hashmap, entry);
                int res = SUCCESS;
                send_msg(state->sock,  (char*)&res, 1, req->client);
                return SUCCESS;
            }
        }
    }
    int res;
    send_msg(state->sock,  (char*)&res, 1, req->client);
    return INVALID_OP;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct application *state = application_new();
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    LearnerCtx *learner_ctx = make_learner(conf);
    set_app_ctx(learner_ctx, state);
    register_deliver_cb(learner_ctx, deliver);
    event_base_dispatch(learner_ctx->base);
    free_learner(learner_ctx);
    application_destroy(state);
    free(conf);
    return (EXIT_SUCCESS);
}