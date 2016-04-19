#include <leveldb/c.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "learner.h"
#include "config.h"


struct client_state {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} client_state;

struct client_state* client_state_new() {
    struct client_state *state = malloc(sizeof(struct client_state));
    char *err = NULL;
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

void *deliver(char* value, void *arg) {
    struct client_state *state = (struct client_state *)arg;
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
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    char *err = NULL;
    char *read;
    size_t read_len;

    char mykey[] = "long key";
    char myval[] = "long valueeeee";
    /******************************************/
    /* OPEN */

    struct client_state *state = client_state_new();
    /******************************************/
    /* WRITE */

    leveldb_put(state->db, state->woptions, mykey, sizeof(mykey), myval, sizeof(myval), &err);

    if (err != NULL) {
      fprintf(stderr, "Write fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;

    /******************************************/
    /* READ */

    read = leveldb_get(state->db, state->roptions, mykey, sizeof(mykey), &read_len, &err);

    if (err != NULL) {
      fprintf(stderr, "Read fail.\n");
      return(1);
    }

    printf("%s: %s\n", mykey, read);

    leveldb_free(err); err = NULL;

    /******************************************/
    /* DELETE */

    // leveldb_delete(db, woptions, mykey, sizeof(mykey), &err);

    // if (err != NULL) {
    //   fprintf(stderr, "Delete fail.\n");
    //   return(1);
    // }

    // leveldb_free(err); err = NULL;

    /******************************************/
    /* CLOSE */

    // leveldb_close(db);

    /******************************************/
    /* DESTROY */

    // leveldb_destroy_db(options, "/tmp/testdb", &err);

    // if (err != NULL) {
    //   fprintf(stderr, "Destroy fail.\n");
    //   return(1);
    // }

    // leveldb_free(err); err = NULL;

    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    start_learner(conf, deliver, state);
    free(conf);
    return (EXIT_SUCCESS);
}