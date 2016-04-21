#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "uthash.h"

struct kv_entry {
    const char *id;                    /* key */
    char value[10];
    UT_hash_handle hh;         /* makes this structure hashable */
};


void add_entry(struct kv_entry **hashmap, char *key, char *value) {
    struct kv_entry *s;
    HASH_FIND_STR(*hashmap, key, s);
    if (s==NULL) {
        s = malloc(sizeof(struct kv_entry));
        s->id = key;
        HASH_ADD_KEYPTR( hh, *hashmap, s->id, strlen(s->id), s );  /* id: value of key field */
    }
    strcpy(s->value, value);
}


struct kv_entry *find_entry(struct kv_entry **hashmap, char *key) {
    struct kv_entry *s;
    HASH_FIND_STR( *hashmap, key, s );  /* s: output pointer */
    return s;
}


void delete_entry(struct kv_entry **hashmap, struct kv_entry *user) {
    HASH_DEL( *hashmap, user);  /* user: pointer to deletee */
    free(user);              /* optional; it's up to you! */
}


int main(int argc, char* argv[]) {
    struct kv_entry *hashmap = NULL;    /* important! initialize to NULL */
    char key[] = "my_id";
    add_entry(&hashmap, key, "Tu Dang");
    struct kv_entry *user = find_entry(&hashmap, key);
    if (user) {
        printf("uservalue is: %s\n", user->value);
    }
    delete_entry(&hashmap, user);
}