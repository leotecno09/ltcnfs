#ifndef STORE_H
#define STORE_H

#include <stddef.h>

#define HASH_HEX_LEN 65 /* 64 hex characters with '\0' */

/*
    create, if not exists, root/ and root/objects/. 
    0 = ok, -1 = error
*/
int store_init(const char *root);

/*
    save data[0... len) and writes in hash_hex the SHA-256 hash.
    If the objects already exists returns 0.
    0 = ok, -1 = error, 1 = already exists (deduplication)
*/
int store_put(const void *data, size_t len, char hash_hex[HASH_HEX_LEN]);

/*
    read the object. Allocates *data with malloc: the caller has to do free().
    0 = ok, -1 = error
*/
int store_get(const char *hash_hex, void **data, size_t *len);

/*
    returns 1 if the object exists, 0 if not.
*/
int store_exists(const char *hash_hex);

/*
    make a snapshot of path (it generates a tree as an object for the store)
    0 = ok, -1 = error
*/
int snapshot_dir(const char *path);
#endif