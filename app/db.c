#include <leveldb/c.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "learner.h"
#include "config.h"


struct application {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} application;

struct application* application_new() {
    struct application *state = malloc(sizeof(struct application));
    char *err = NULL;
    /******************************************/
    /* OPEN DB */
    state->options = leveldb_options_create();
    leveldb_options_set_create_if_missing(state->options, 1);
    state->db = leveldb_open(state->options, "/tmp/testdb", &err);
    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      exit(EXIT_FAILURE);
    }
    /* reset error var */
    leveldb_free(err); err = NULL;
    state->woptions = leveldb_writeoptions_create();
    state->roptions = leveldb_readoptions_create();

    return state;
}

void application_destroy(struct application *state) {
    char *err = NULL;
    leveldb_close(state->db);
    leveldb_destroy_db(state->options, "/tmp/testdb", &err);
    if (err != NULL) {
      fprintf(stderr, "Destroy fail.\n");
      return;
    }
    leveldb_free(err); err = NULL;

}

void *deliver(const char* chosen, void *arg) {
    struct application *state = (struct application *)arg;
    // printf("delivered %s\n", chosen);
    if (!chosen || chosen[0] == '\0') {
        return NULL;
    }
    char *err = NULL;
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
        size_t keylen = strlen(key) + 1;
        size_t vallen = strlen(val) + 1;
        // printf("PUT (%s:%zu, %s:%zu)\n", key, keylen, val, vallen);
        leveldb_put(state->db, state->woptions, key, keylen, val, vallen, &err);
        if (err != NULL) {
            free(chosen_duplicate);
            return err;
        }
        leveldb_free(err); err = NULL;
        return strdup("PUT OK");
    }
    else if (strcmp(token, "GET") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        int keylen = strlen(key) + 1;
        // printf("PUT (%s:%zu)\n", key, keylen);
        char *val = leveldb_get(state->db, state->roptions, key, keylen, &read_len, &err);
        if (err != NULL) {
            free(chosen_duplicate);
            return err;
        }
        leveldb_free(err); err = NULL;
        // printf("%s: %s\n", key, val);
        if (!val) {
            free(chosen_duplicate);
            return strdup("NOT FOUND");
        }
        return val;
    }
    else if (strcmp(token, "DEL") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        int keylen = strlen(key) + 1;
        leveldb_delete(state->db, state->woptions, key, keylen, &err);
        if (err != NULL) {
            free(chosen_duplicate);
            return err;
        }
        leveldb_free(err); err = NULL;
        free(chosen_duplicate);
        return strdup("DELETE OK");
    }
    return NULL;
}


int main(int argc, char* argv[]) {
    char *err = NULL;
    char *read;
    size_t read_len;

    struct application *state = application_new();
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