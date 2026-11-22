// funziona ma i tree sono rotti

#include "store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sodium.h>
#include <dirent.h>

static char store_root[PATH_MAX];

static void to_hex(const unsigned char *bin, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) {
        sprintf(out + 2 * i, "%02x", bin[i]);
    }
}

static void object_path(const char *hash_hex, char *out, size_t size) {
    snprintf(out, size, "%s/objects/%.2s/%s", store_root, hash_hex, hash_hex + 2);
}

/* writes buf into fd */
static int write_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = buf;

    while (len > 0) {
        ssize_t w = write(fd, p, len);
        if (w == -1) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        p += w;
        len -= w;
    }

    return 0;
}

/* read from fd into buf */
static int read_all(int fd, void *buf, size_t len) {
    unsigned char *p = buf;

    while (len > 0) {
        ssize_t r = read(fd, p, len);

        if (r == -1) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (r == 0)
            return -1;      // reached EOF

        p += r;
        len -= r;
    }

    return 0;
}

static int buffer_append(void **buf, const void *in, size_t *buf_size, size_t *buf_len, size_t in_size) {
    size_t required = *buf_len + in_size;

    if (required > *buf_size) {
        size_t new_size = *buf_size ? *buf_size : 256;

        while (new_size < required) {
            if (new_size > SIZE_MAX / 2) {
                new_size = required;
                break;
            }

            new_size *= 2;
        }

        void *tmp = realloc(*buf, new_size);
        if (tmp == NULL) {
            perror("realloc failed");
            return -1;
        }

        *buf = tmp;
        *buf_size = new_size;
    }

    memcpy((char *)*buf + *buf_len, in, in_size);
    *buf_len += in_size;

    return 0;
}

int store_init(const char *root) {
    char objects[PATH_MAX];
    int n;

    if (sodium_init() < 0) {
        fprintf(stderr, "sodium_init failed\n");
        return -1;
    }

    n = snprintf(store_root, sizeof store_root, "%s", root);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof store_root)
        return -1;

    n = snprintf(objects, sizeof objects, "%s/objects", store_root);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof objects)
        return -1;  

    if (mkdir(store_root, 0755) == -1 && errno != EEXIST)
        return -1;

    if (mkdir(objects, 0755) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

int store_exists(const char *hash_hex) {
    char path[PATH_MAX];
    struct stat st;

    object_path(hash_hex, path, sizeof path);

    return stat(path, &st) == 0;
}

int store_put(const void *data, size_t len, char hash_hex[HASH_HEX_LEN]) {
    unsigned char hash[crypto_hash_sha256_BYTES];
    //int n;

    crypto_hash_sha256(hash, data, len);        // <-- questa calcola l'hash del file
    to_hex(hash, sizeof hash, hash_hex);                // <-- questa lo converte in hex

    if (store_exists(hash_hex))         // deduplication
        return 1;

    // we put the blob into the store
    char dir[PATH_MAX];
    snprintf(dir, sizeof dir, "%s/objects/%.2s", store_root, hash_hex);

    printf("dir=%s\n", dir);

    if (mkdir(dir, 0755) == -1 && errno != EEXIST)      /* if it exists it's not a problem */
        return -1;

    char tmp_file[PATH_MAX];
    snprintf(tmp_file, sizeof tmp_file, "%s/objects/tmp_XXXXXX", store_root);

    printf("tmp_file=%s\n", tmp_file);

    int tmp_fd = mkstemp(tmp_file);
    if (tmp_fd == -1) {
        perror("mkstemp failed");
        return -1;
    }

    if (write_all(tmp_fd, data, len) == -1) {
        perror("write failed");
        close(tmp_fd);
        unlink(tmp_file);
        
        return -1;
    }

    close(tmp_fd);

    char final_path[PATH_MAX];
    object_path(hash_hex, final_path, sizeof final_path);

    if (rename(tmp_file, final_path) != 0) {
        perror("rename failed");
        unlink(tmp_file);

        return -1;
    }

    unlink(tmp_file);
    return 0;
}

int store_get(const char *hash_hex, void **data, size_t *len) {
    // CONTROLLARE CHE HASH_HEX SIA VALIDO!!

    char path[PATH_MAX];
    struct stat sb;

    if (hash_hex == NULL || data == NULL || len == NULL)
        return -1;

    object_path(hash_hex, path, sizeof path);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }

    if (fstat(fd, &sb) == -1) {
        perror("fstat failed");
        close(fd);
        return -1;
    }     // now the size can be retreived from sb.st_size

    if (sb.st_size < 0 || (uintmax_t)sb.st_size > SIZE_MAX) {
        close(fd);
        return -1;
    }

    *data = malloc((size_t)sb.st_size);
    if (*data == NULL) {
        perror("malloc failed");
        close(fd);

        return -1;
    }

    if(read_all(fd, *data, sb.st_size) == -1) {
        perror("read failed");
        free(*data);
        close(fd);

        return -1;
    }

    *len = (size_t)sb.st_size;

    close(fd);

    return 0;
}

int snapshot_dir(const char *path) {
    char *buf = NULL;
    size_t buf_size = 0;
    size_t buf_len = 0;

    char tree_hash[HASH_HEX_LEN];

    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir failed");
        return -1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // entry->d_name is the element name
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        int n = snprintf(full_path, sizeof full_path, "%s/%s", path, entry->d_name);

        if (n < 0 || (size_t)n >= sizeof full_path)
            continue;

        struct stat sb;

        size_t entry_len = 4 + 1 + (HASH_HEX_LEN - 1) + 1 + strlen(entry->d_name) + 1 + 1;  //  tree b058d... file.txt\n\0

        char *tree_entry = malloc(entry_len);
        if (tree_entry == NULL) {
            perror("entry buffer malloc failed");
            closedir(dir);

            return -1;
        }

        if (lstat(full_path, &sb) == -1) {
            perror("lstat failed");

            //closedir(dir);
            free(tree_entry);

            continue;
        }

        if (S_ISREG(sb.st_mode)) {  // it's a file
            void *file_content = malloc((size_t)sb.st_size);
            if (file_content == NULL) {
                perror("malloc file_content failed");
                closedir(dir);
                return -1;
            }

            char hash_hex[HASH_HEX_LEN];

            int fd = open(full_path, O_RDONLY);
            if (fd == -1) {
                perror("file open failed");

                free(file_content);
                free(tree_entry);

                closedir(dir);

                return -1;
            }

            if (read_all(fd, file_content, (size_t)sb.st_size) == -1) {
                perror("read file failed");
                close(fd);
                closedir(dir);

                free(file_content);
                free(tree_entry);

                return -1;
            }

            close(fd);

            if (store_put(file_content, sb.st_size, hash_hex) == -1) {
                perror("store_put failed");
                //close(fd);
                closedir(dir);

                free(file_content);
                free(tree_entry);

                return -1;
            }

            free(file_content);

            int n = snprintf(tree_entry, entry_len, "blob %s %s\n", hash_hex, entry->d_name);
            if (n < 0 || (size_t)n >= entry_len) {
                perror("snprintf failed");

                //close(fd);
                closedir(dir);
                free(tree_entry);
                //free(file_content);

                return -1;
            }

            if (buffer_append((void **)&buf, tree_entry, &buf_size, &buf_len, strlen(tree_entry)) == -1) {
                perror("buffer_append failed");
                free(buf);
                free(tree_entry);
                closedir(dir);

                return -1;
            }

            free(tree_entry);
        }

        else if (S_ISDIR(sb.st_mode)) {
            snapshot_dir(full_path);

            int n = snprintf(tree_entry, entry_len, "tree %s %s\n", tree_hash, entry->d_name);
            if (n < 0 || (size_t)n >= entry_len) {
                perror("snprintf failed");

                //close(fd);
                closedir(dir);
                free(tree_entry);
                //free(file_content);

                return -1;
            }

            if (buffer_append((void **)&buf, tree_entry, &buf_size, &buf_len, strlen(tree_entry)) == -1) {
                perror("buffer_append failed");
                free(buf);
                free(tree_entry);
                closedir(dir);

                return -1;
            }

            free(tree_entry);
        }
    }

    if (store_put(buf, buf_size, tree_hash) == -1) {
        perror("store_put failed");
        free(buf);
        closedir(dir);

        return -1;
    }

    free(buf);

    closedir(dir);
    return 0;
}

/*
    snapshot_dir(path) -> hash:

    alloca un buffer che sarà il contenuto del tree
    per ogni elemento nel path:
        se è un file normale: 
            legge il file e lo passa a store_put()
            aggiunge al buffer "blob    h   nome" (h = hash_hex di store_put)
        se è una cartella:
            richiama snapshot_dir()
            aggiunge al buffer "tree    h   nome" (h = hash_hex di snapshot_dir)

    return store_put(buffer)

    Dimensione buffer calcolata con:
    size_t entry_len = 4 (tree o blob) + 1 (separatore) + (HASH_HEX_LEN - 1) + 1 (separatore) + strlen(name) + 1;
*/