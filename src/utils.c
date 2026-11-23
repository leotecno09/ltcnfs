#include "utils.h"

/* writes buf into fd */
int write_all(int fd, const void *buf, size_t len) {
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
int read_all(int fd, void *buf, size_t len) {
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

int buffer_append(void **buf, const void *in, size_t *buf_size, size_t *buf_len, size_t in_size) {
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