#include <leveldb/c.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include "netpaxos_utils.h"

enum boolean { false, true };

struct data {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    int mps;
};

void open_db(struct data *ctx, char* db_name) {
    char *err = NULL;
    ctx->options = leveldb_options_create();
    leveldb_options_set_create_if_missing(ctx->options, true);
    ctx->db = leveldb_open(ctx->options,db_name, &err);    
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

void destroy_db(struct data *ctx, char* db_name) {
    char *err = NULL;
    leveldb_destroy_db(ctx->options, db_name, &err);    
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

int add_entry(struct data *ctx, int sync, char *key, int ksize, char* val, int vsize) {
    char *err = NULL;
    leveldb_writeoptions_set_sync(ctx->woptions, sync);
    leveldb_put(ctx->db, ctx->woptions, key, ksize, val, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Write fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int get_value(struct data *ctx, char *key, size_t ksize, char** val, size_t* vsize) {
    char *err = NULL;
    *val = leveldb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Read fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int delete_entry(struct data *ctx, char *key, int ksize) {
    char *err = NULL;
    leveldb_delete(ctx->db, ctx->woptions, key, ksize, &err);
    if (err != NULL) {
        fprintf(stderr, "Delete fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

struct data *new_data() {
    struct data *ctx = malloc(sizeof(struct data));
    ctx->woptions = leveldb_writeoptions_create();
    ctx->roptions = leveldb_readoptions_create();
    ctx->mps = 0;
    return ctx;
}

void free_data(struct data *ctx) {
    leveldb_writeoptions_destroy(ctx->woptions);
    leveldb_readoptions_destroy(ctx->roptions);
    leveldb_options_destroy(ctx->options);
}

void *monitor(void *arg)
{
    int  *mps = arg;
    while(true) {
        printf("%d\n", *mps);
        *mps = 0;
        sleep(1);
    }

    return NULL;
}

static char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK...";
    if (size) {
        --size;
        size_t n;
        for ( n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}


char **generate_array(int n, size_t ksize) {
    char **keys = calloc(n, sizeof(char*));
    int i;
    for (i = 0; i < n; i++) {
        keys[i] = malloc(ksize);
        assert(keys[i] != NULL);
        rand_string(keys[i], ksize);
    }
    return keys;
}

void free_generated_array(char **keys, int n) {
    int i;
    for (i = 0; i < n; i++) {
        free(keys[i]);
    }
    free(keys);
}

void fillsync(struct data *ctx, int num_elements, int num_keys, char** keys, size_t ksize, char**values, size_t vsize) {
    int i;
    int sync = true;
    for (i = 0; i < num_elements; i++) {
        int idx = i % num_keys;
        add_entry(ctx, sync, keys[idx], ksize, values[idx], vsize);
    }
}

void fillrandom(struct data *ctx, int num_elements, int num_keys, char** keys, size_t ksize, char**values, size_t vsize) {
    int i;
    int sync = false;
    for (i = 0; i < num_elements; i++) {
        int idx = i % num_keys;
        add_entry(ctx, sync, keys[idx], ksize, values[idx], vsize);
    }
}

void read_random(struct data *ctx, int num_elements, int num_keys, char** keys, size_t ksize) {
    int i;
    for (i = 0; i < num_elements; i++) {
        int idx = i % num_keys;
        char *value = NULL;
        size_t vsize;
        get_value(ctx, keys[idx], ksize, &value, &vsize);
        if (value) {
            // printf("%d: key: %s, value: %s\n", i, keys[idx], value);
            free(value);
        }
    }
}

int main() {
    struct data *sync_ctx = new_data();

    int n = 10;
    size_t size = 16;
    char **keys = generate_array(n, size);
    assert(keys != NULL);
    char **values = generate_array(n, size);
    assert(values != NULL);

    int num_sync_element = 10000;
    int num_elements = 1000000;

    printf("%-15s: version %d.%d\n", "LevelDB:", leveldb_major_version(), leveldb_minor_version());
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    printf("%-15s: %s", "Date:", asctime (timeinfo));
    printf("%-15s: %zu bytes each\n", "Keys:", size);
    printf("%-15s: %zu bytes each\n", "Values:", size);
    printf("%-15s: %d bytes each\n", "Entries:", num_elements);
    char cross[51];
    memset(cross, '-', 50);
    cross[50] = '\0';
    printf("%-50s\n", cross);

    struct timespec start, end, result;
    gettime(&start);

    open_db(sync_ctx, "/tmp/leveldb_fillsync");

    fillsync(sync_ctx, num_sync_element, n, keys, size, values, size);
    gettime(&end);
    timediff(&result, &end, &start);
    double latency = (result.tv_sec * 1e9 + result.tv_nsec);
    printf("%-15s: %12.2f ns/op; (%d ops/s)\n", "fillsync", latency/num_sync_element, (int)(1e9*num_sync_element/latency) );
    
    // close db
    leveldb_close(sync_ctx->db);


    struct data *cached_ctx = new_data();
    open_db(cached_ctx, "/tmp/leveldb_fillcached");

    gettime(&start);
    fillrandom(cached_ctx, num_elements, n, keys, size, values, size);
    gettime(&end);
    timediff(&result, &end, &start);
    latency = (result.tv_sec * 1e9 + result.tv_nsec);
    printf("%-15s: %12.2f ns/op; (%d ops/s)\n", "fillrandom", latency/num_elements, (int)(1e9*num_elements/latency) );

    gettime(&start);
    read_random(cached_ctx, num_elements, n, keys, size);
    gettime(&end);
    timediff(&result, &end, &start);
    latency = (result.tv_sec * 1e9 + result.tv_nsec);
    printf("%-15s: %12.2f ns/op; (%d ops/s)\n", "readrandom", latency/num_elements, (int)(1e9*num_elements/latency) );


    leveldb_close(cached_ctx->db);
    destroy_db(sync_ctx, "/tmp/leveldb_fillsync");
    destroy_db(cached_ctx, "/tmp/leveldb_fillcached");


    free_generated_array(keys, n);
    free_generated_array(values, n);
    free_data(sync_ctx);
    free_data(cached_ctx);
    
    return(EXIT_SUCCESS);
}