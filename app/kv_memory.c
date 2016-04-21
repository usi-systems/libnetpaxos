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

void add_entry(struct kv_entry **hashmap, char *key, char *value) {
    struct kv_entry *s;
    HASH_FIND_STR(*hashmap, key, s);
    if (s==NULL) {
        s = malloc(sizeof(struct kv_entry));
        s->key = key;
        HASH_ADD_KEYPTR( hh, *hashmap, s->key, strlen(s->key), s );  /* id: value of key field */
    }
    strcpy(s->value, value);
}


struct kv_entry *find_entry(struct kv_entry **hashmap, char *key) {
    struct kv_entry *s;
    HASH_FIND_STR( *hashmap, key, s );  /* s: output pointer */
    return s;
}


void delete_entry(struct kv_entry **hashmap, struct kv_entry *entry) {
    HASH_DEL( *hashmap, entry);  /* entry: pointer to deletee */
    free(entry);              /* optional; it's up to you! */
}

void *deliver(const char* request, void *arg) {
    struct application *state = arg;
    if (!request || request[0] == '\0') {
        return NULL;
    }
    size_t read_len;
    char op = request[0];
    switch(op) {
        case 'P': {
            unsigned char ksize = request[1];
            unsigned char vsize = request[2];
            char key[ksize+1];
            memcpy(key, &request[3], ksize);
            char value[vsize+1];
            memcpy(value, &request[3 + ksize], vsize);
            add_entry(&state->hashmap, key, value);
            return strdup("SUCCESS");
        }
        case 'G': {
            unsigned char ksize = request[1];
            char key[ksize+1];
            memcpy(key, &request[3], ksize);
            struct kv_entry *entry = find_entry(&state->hashmap, key);
            if (entry) {
                char *val = strdup(entry->value);
                return val;
            }
        }
        case 'D': {
            unsigned char ksize = request[1];
            char key[ksize+1];
            memcpy(key, &request[3], ksize);
            struct kv_entry *entry = find_entry(&state->hashmap, key);
            if (entry) {
                delete_entry(&state->hashmap, entry);
                return strdup("DELETE OK");
            }
        }
    }
    return NULL;
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