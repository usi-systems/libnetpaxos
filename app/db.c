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

void *deliver(char* value, void *arg) {
    struct application *state = (struct application *)arg;
    // printf("delivered %s\n", value);
    char* token = strtok(value, " ");
    char *err = NULL;
    size_t read_len;

    if (strcmp(token, "PUT") == 0) {
        char *key = strtok(NULL, " ");
        char *val = strtok(NULL, " ");
        int keylen = strlen(key) + 1;
        int vallen = strlen(val) + 1;
        leveldb_put(state->db, state->woptions, key, keylen, val, vallen, &err);
        if (err != NULL) {
            fprintf(stderr, "Write fail.\n");
            return;
        }
        leveldb_free(err); err = NULL;
        return strdup("PUT OK");
    }
    else if (strcmp(token, "GET") == 0) {
        char *key = strtok(NULL, " ");
        int keylen = strlen(key) + 1;
        char *val = leveldb_get(state->db, state->roptions, key, keylen, &read_len, &err);
        if (err != NULL) {
            fprintf(stderr, "Read fail.\n");
            return;
        }
        leveldb_free(err); err = NULL;
        // printf("%s: %s\n", key, val);
        if (val) {
            return val;
        } else
            return strdup("NOT FOUND");
    }
    else if (strcmp(token, "DEL") == 0) {
        char *key = strtok(NULL, " ");
        int keylen = strlen(key) + 1;
        leveldb_delete(state->db, state->woptions, key, keylen, &err);
        if (err != NULL) {
            fprintf(stderr, "DELETE fail.\n");
            return;
        }
        leveldb_free(err); err = NULL;
         // printf("DELETED %s\n", key);
        char *res = strdup("DELETE OK");
        return res;
    }
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