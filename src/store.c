#include "repo.h"
#include "store.h"
#include "utils.h"
#include "branch.h"
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
#include <time.h>

//static char store_root[PATH_MAX];

static void to_hex(const unsigned char *bin, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) {
        sprintf(out + 2 * i, "%02x", bin[i]);
    }
}

int store_init(struct repo *r, const char *root) {
    /*
        Serve creare il branch "master" e far puntare HEAD ad esso.
        Bisogna anche riempire gli altri campi della struttura repo, tra qui "current_branch"
    */

    char objects[PATH_MAX];
    char refs[PATH_MAX];
    char branches[PATH_MAX];
    int n;

    if (sodium_init() < 0) {
        fprintf(stderr, "sodium_init failed\n");
        return -1;
    }

    n = snprintf(r->root, sizeof r->root, "%s", root);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof r->root)
        return -1;

    n = snprintf(objects, sizeof objects, "%s/objects", r->root);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof objects)
        return -1;  

    if (mkdir(r->root, 0755) == -1 && errno != EEXIST)
        return -1;

    if (mkdir(objects, 0755) == -1 && errno != EEXIST)
        return -1;

    // create the refs folder
    n = snprintf(refs, sizeof refs, "%s/refs", r->root);

    //printf("[DEBUG] refs=%s\n", refs);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof refs) {
        fprintf(stderr, "error: the path of refs was truncated\n");
        return -1;
    } 

    if (mkdir(refs, 0755) == -1 && errno != EEXIST) {
        perror("[DEBUG] mkdir failed");
        return -1;
    }

    // create the branches folder
    n = snprintf(branches, sizeof branches, "%s/branches", refs);
  
    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof branches) {
        fprintf(stderr, "error: the path of refs was truncated\n");
        return -1;
    } 

    if (mkdir(branches, 0755) == -1 && errno != EEXIST) {
        perror("[DEBUG] mkdir failed");
        return -1;
    }

    // create the master branch
    if (new_branch(r, "master", NULL) == -1) {
        perror("master branch creation failed");
        return -1;
    }

    if (checkout(r, "master") == -1) {
        perror("checkout on master failed");
        return -1;
    }

    return 0;
}

int store_exists(const struct repo *r, const char *hash_hex) {
    char path[PATH_MAX];
    struct stat st;

    object_path(r, hash_hex, path, sizeof path);

    return stat(path, &st) == 0;
}

int store_put(const struct repo *r, const void *data, size_t len, char hash_hex[HASH_HEX_LEN]) {
    unsigned char hash[crypto_hash_sha256_BYTES];
    //int n;

    crypto_hash_sha256(hash, data, len);        // <-- questa calcola l'hash del file
    to_hex(hash, sizeof hash, hash_hex);                // <-- questa lo converte in hex

    if (store_exists(r, hash_hex))         // deduplication
        return 1;

    // we put the blob into the store
    char dir[PATH_MAX];
    snprintf(dir, sizeof dir, "%s/objects/%.2s", r->root, hash_hex);

    //printf("dir=%s\n", dir);

    if (mkdir(dir, 0755) == -1 && errno != EEXIST)      /* if it exists it's not a problem */
        return -1;

    char tmp_file[PATH_MAX];
    snprintf(tmp_file, sizeof tmp_file, "%s/objects/tmp_XXXXXX", r->root);

    //printf("tmp_file=%s\n", tmp_file);

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
    object_path(r, hash_hex, final_path, sizeof final_path);

    if (rename(tmp_file, final_path) != 0) {
        perror("rename failed");
        unlink(tmp_file);

        return -1;
    }

    unlink(tmp_file);
    return 0;
}

int store_get(const struct repo *r, const char *hash_hex, void **data, size_t *len) {
    // CONTROLLARE CHE HASH_HEX SIA VALIDO!!

    char path[PATH_MAX];
    struct stat sb;

    if (hash_hex == NULL || data == NULL || len == NULL)
        return -1;

    object_path(r, hash_hex, path, sizeof path);

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

int snapshot_dir(const struct repo *r, const char *path, char cascade_tree_hash[HASH_HEX_LEN], struct snapshot_report *sr) {
    void *buf = NULL;
    size_t buf_size = 0;        // total allocated size
    size_t buf_len = 0;         // used size

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

        printf("[DEBUG] now working on %s\n", entry->d_name);

        char full_path[PATH_MAX];
        int n = snprintf(full_path, sizeof full_path, "%s/%s", path, entry->d_name);

        if (n < 0 || (size_t)n >= sizeof full_path)
            continue;

        struct stat sb;

        size_t entry_len = 4 + 1 + (HASH_HEX_LEN - 1) + 1 + strlen(entry->d_name) + 1 + 1;  //  blob b058d... file.txt\n\0

        char *tree_entry = malloc(entry_len);
        if (tree_entry == NULL) {
            perror("entry buffer malloc failed");
            closedir(dir);

            return -1;
        }

        if (lstat(full_path, &sb) == -1) {          // ignora i link simbolici, da risolvere!! (leaka anche tree_entry per questo)
            perror("lstat failed");

            //closedir(dir);
            free(tree_entry);

            continue;
        }

        if (S_ISREG(sb.st_mode)) {  // it's a file
            void *file_content = malloc((size_t)sb.st_size);        // ma pk nn allocare dopo che il file è stato aperto?
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
                //free(buf);

                //closedir(dir);
                sr->sr_skipped++;

                continue;
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

            if (store_put(r, file_content, sb.st_size, hash_hex) == -1) {
                perror("store_put failed");
                //close(fd);
                closedir(dir);

                free(file_content);
                free(tree_entry);

                return -1;
            }

            free(file_content);

            int n = snprintf(tree_entry, entry_len, "blob %s %s\n", hash_hex, entry->d_name);

            printf("[DEBUG] tree_entry=%s\n", tree_entry);

            if (n < 0 || (size_t)n >= entry_len) {
                perror("snprintf failed");

                //close(fd);
                closedir(dir);
                free(tree_entry);
                //free(file_content);

                return -1;
            }

            if (buffer_append(&buf, tree_entry, &buf_size, &buf_len, strlen(tree_entry)) == -1) {
                perror("buffer_append failed");
                free(buf);
                free(tree_entry);
                closedir(dir);

                return -1;
            }

            free(tree_entry);

            sr->sr_files++;
        }

        else if (S_ISDIR(sb.st_mode)) {
            snapshot_dir(r, full_path, tree_hash, sr);

            //printf("[DEBUG] tree_hash=%s\n", tree_hash);

            int n = snprintf(tree_entry, entry_len, "tree %s %s\n", tree_hash, entry->d_name);
            if (n < 0 || (size_t)n >= entry_len) {
                perror("snprintf failed");

                //close(fd);
                closedir(dir);
                free(tree_entry);
                //free(file_content);

                return -1;
            }

            if (buffer_append(&buf, tree_entry, &buf_size, &buf_len, strlen(tree_entry)) == -1) {
                perror("buffer_append failed");
                free(buf);
                free(tree_entry);
                closedir(dir);

                return -1;
            }

            free(tree_entry);

            sr->sr_dirs++;
        } else {
            // file not recognized

            free(tree_entry);
            sr->sr_skipped++;
            continue;
        }
    }

    /*
        buf_len is the allocated size of the buffer
        buf_size is the total size (which includes unused areas after the realloc, garbage data)
    */
    if (store_put(r, buf, buf_len, tree_hash) == -1) {
        perror("store_put failed");
        free(buf);
        closedir(dir);

        return -1;
    }

    //if (cascade_tree_hash)              // cascade_tree_hash verrà usato poi dal chiamante per fare il commit
    memcpy(cascade_tree_hash, tree_hash, HASH_HEX_LEN);

    free(buf);

    closedir(dir);
    return 0;
}

int commit(const struct repo *r, char tree_root_hash[HASH_HEX_LEN], const char *msg) {
    // generates a commit, the commit hash will be saved in the current branch file
    // commit content is: tree <tree_hash>\nparent\n<parent_commit_hash>\ntime\n<time>\nmessage <msg>
    // magari limitare lunghezza msg
    time_t curtime;
    time(&curtime);

    void *buf = NULL;
    size_t buf_size = 0, buf_len = 0;
    char line[128];
    int n;

    n = snprintf(line, sizeof line, "tree %s\n", tree_root_hash);
    if (n < 0 || (size_t)n >= sizeof line) goto fail;
    if (buffer_append(&buf, line, &buf_size, &buf_len, (size_t)n) == -1) goto fail;

    if (strlen(r->last_commit) > 0) {
        n = snprintf(line, sizeof line, "parent %s\n", r->last_commit);
        if (n < 0 || (size_t)n >= sizeof line) goto fail;
        if (buffer_append(&buf, line, &buf_size, &buf_len, (size_t)n) == -1) goto fail;
    }

    n = snprintf(line, sizeof line, "time %lld\n\nmessage %s\n", curtime, msg);
    if (n < 0 || (size_t)n >= sizeof line) goto fail;
    if (buffer_append(&buf, line, &buf_size, &buf_len, (size_t)n) == -1) goto fail;

    // calcolare hash e mettere nello store con store_put
    if (store_put(r, buf, buf_len, r->last_commit) == -1) goto fail;
    if (branch_update(r, r->current_branch, r->last_commit) == -1) goto fail;

    free(buf);

    return 0;

fail:
    perror("commit failed");
    free(buf);
    return -1;
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