#include <leveldb/c.h>
#include <stdio.h>
#include <string.h>

int main() {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    char *err = NULL;
    char *read;
    size_t read_len;

    char mykey[] = "long key";
    char myval[] = "long valueeeee";
    /******************************************/
    /* OPEN */

    printf("sizeof :%lu %lu\n", sizeof(mykey), sizeof(myval));
    printf("strlen :%zu %zu\n", strlen(mykey), strlen(myval));
    options = leveldb_options_create();
    leveldb_options_set_create_if_missing(options, 1);
    db = leveldb_open(options, "/tmp/testdb", &err);

    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      return(1);
    }

    /* reset error var */
    leveldb_free(err); err = NULL;

    /******************************************/
    /* WRITE */

    woptions = leveldb_writeoptions_create();
    leveldb_put(db, woptions, mykey, sizeof(mykey), myval, sizeof(myval), &err);

    if (err != NULL) {
      fprintf(stderr, "Write fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;

    /******************************************/
    /* READ */

    roptions = leveldb_readoptions_create();
    read = leveldb_get(db, roptions, mykey, sizeof(mykey), &read_len, &err);

    if (err != NULL) {
      fprintf(stderr, "Read fail.\n");
      return(1);
    }

    printf("%s: %s\n", mykey, read);

    leveldb_free(err); err = NULL;

    /******************************************/
    /* DELETE */

    leveldb_delete(db, woptions, mykey, sizeof(mykey), &err);

    if (err != NULL) {
      fprintf(stderr, "Delete fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;

    /******************************************/
    /* CLOSE */

    leveldb_close(db);

    /******************************************/
    /* DESTROY */

    leveldb_destroy_db(options, "/tmp/testdb", &err);

    if (err != NULL) {
      fprintf(stderr, "Destroy fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;


    return(0);
}