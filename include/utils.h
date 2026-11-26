#ifndef _LTCNFS_UTILS
#define _LTCNFS_UTILS

/*
    writes buf into a file using the file descriptor
*/
int write_all(int fd, const void *buf, size_t len);

/*
    read data from a file and puts int into buf, the buffer needs to be already allocated by the caller
*/
int read_all(int fd, void *buf, size_t len);

/*
    append data from in to buf, it automatically reallocate buf if needed.
    buf_len will be the used size
    buf_size will be the total allocated size (which may include garbage data)
*/
int buffer_append(void **buf, const void *in, size_t *buf_size, size_t *buf_len, size_t in_size);


/*
    build the path from the store root
*/
int repo_path(const struct repo *r, char *out, size_t size, const char *fmt, ...);

/*
    get the full path of an object starting from its hash
*/
void object_path(const struct repo *r, const char *hash_hex, char *out, size_t size);
#endif