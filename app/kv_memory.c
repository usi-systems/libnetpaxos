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

void *deliver(const char* chosen, void *arg) {
    struct application *state = arg;
    // printf("delivered %s\n", chosen);
    if (!chosen || chosen[0] == '\0') {
        return NULL;
    }
    char *chosen_duplicate = strdup(chosen);
    char* token = strtok(chosen_duplicate, " ");
    size_t read_len;
    if (strcmp(token, "PUT") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        char *val = strtok(NULL, " ");
        if (!val) {
            free(chosen_duplicate);
            return strdup("Value is NULL");
        }
        add_entry(&state->hashmap, key, val);
        return strdup("PUT OK");
    }
    else if (strcmp(token, "GET") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        struct kv_entry *entry = find_entry(&state->hashmap, key);
        if (!entry) {
            free(chosen_duplicate);
            return strdup("NOT FOUND");
        }
        char *val = strdup(entry->value);
        return val;
    }
    else if (strcmp(token, "DEL") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        struct kv_entry *entry = find_entry(&state->hashmap, key);
        if (entry) {
            delete_entry(&state->hashmap, entry);
            free(chosen_duplicate);
            return strdup("DELETE OK");
        }  else {
            free(chosen_duplicate);
            return strdup("KEY NOT FOUND");
        }
    }
    return NULL;
}


int main(int argc, char* argv[]) {
    struct application *state = application_new();

    char key[] = "my_id";
    char value[] = "my_value";
    add_entry(&state->hashmap, key, value);
    struct kv_entry *user = find_entry(&state->hashmap, key);
    if (user) {
        printf("hm[%s]=%s\n", user->key, user->value);
    }
    delete_entry(&state->hashmap, user);

    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    start_learner(conf, deliver, state);
    application_destroy(state);
    free(conf);
    return (EXIT_SUCCESS);
}