#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "uthash.h"
#include <stdio.h>
#include <unistd.h>

#include "learner.h"
#include "config.h"

struct kv_entry {
    const char *key;                    /* key */
    char value[10];
    UT_hash_handle hh;         /* makes this structure hashable */
};

struct application {
    struct kv_entry *hashmap;
} application;

struct application* application_new() {
    struct application *state = malloc(sizeof(struct application));
    state->hashmap = NULL;    /* important! initialize to NULL */
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

int deliver(const char* request, void *arg, char **return_val, int *return_vsize) {
    struct application *state = arg;
    if (!request || request[0] == '\0') {
        return FAILED;
    }
    size_t read_len;
    char op = request[0];
    switch(op) {
        case 'P': {
            unsigned char ksize = request[1];
            unsigned char vsize = request[2];
            add_entry(&state->hashmap, &request[3], ksize, &request[3+ksize], vsize);
            return SUCCESS;
        }
        case 'G': {
            unsigned char ksize = request[1];
            struct kv_entry *entry = find_entry(&state->hashmap, &request[3], ksize);
            if (entry) {
                *return_val = strdup(entry->value);
                *return_vsize = strlen(entry->value);
                return GOT_VALUE;
            }
            return NOT_FOUND;
        }
        case 'D': {
            unsigned char ksize = request[1];
            struct kv_entry *entry = find_entry(&state->hashmap, &request[3], ksize);
            if (entry) {
                delete_entry(&state->hashmap, entry);
                return SUCCESS;
            }
        }
    }
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
    start_learner(conf, deliver, state);
    application_destroy(state);
    free(conf);
    return (EXIT_SUCCESS);
}