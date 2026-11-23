#include "branch.h"
#include "utils.h"
#include "repo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

static int head_write(const struct repo *r, const char *new_branch) {
    char HEAD_TMP[PATH_MAX];
    char BRANCH_LOCATION[PATH_MAX];

    repo_path(r, BRANCH_LOCATION, sizeof BRANCH_LOCATION, "refs/branches/%s\n", new_branch);

    struct stat st;
    if (!stat(BRANCH_LOCATION, &st)) {
        perror("branch does not exist");
        return -1;       // magari distinguere per errore "umano"?
    }

    repo_path(r, HEAD_TMP, sizeof HEAD_TMP, "TMPHEAD_XXXXXX");
    int fd = mkstemp(HEAD_TMP);

    if (fd == -1) {
        perror("mkstemp failed");
        return -1;
    }

    if (write_all(fd, BRANCH_LOCATION, strlen(BRANCH_LOCATION)) == -1) {
        perror("write failed on temp branch");
        close(fd);
        unlink(HEAD_TMP);
        return -1;
    }

    close(fd);

    char HEAD_LOCATION[PATH_MAX];
    repo_path(r, HEAD_LOCATION, sizeof HEAD_LOCATION, "HEAD");

    if (rename(HEAD_TMP, HEAD_LOCATION) != 0) {
        perror("renaming HEAD");
        unlink(HEAD_TMP);

        return -1;
    }

    unlink(HEAD_TMP);

    return 0;
}

int branch_update(const struct repo *r, const char *branch_name, const char commit[HASH_HEX_LEN]) {
    // TODO
}

int new_branch(const struct repo *r, const char *name, const char last_commit[HASH_HEX_LEN]) {
    char TMP_FILE[PATH_MAX];

    snprintf(TMP_FILE, sizeof TMP_FILE, "%s/refs/branches/tmpbranch_XXXXXX", r->root);   // ci fidiamo di sta snprintf per ora

    int branchf = mkstemp(TMP_FILE);
    if (branchf == -1) {
        perror("mkstemp failed");
        return -1;
    }

    if (write_all(branchf, last_commit, strlen(last_commit)) == -1) {
        perror("write_all failed");
        close(branchf);
        unlink(TMP_FILE);
    }

    close(branchf);

    char final_path[PATH_MAX];
    int n = snprintf(final_path, sizeof final_path, "%s/refs/branches/%s", r->root, name);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof final_path)
        return -1;

    if (rename(TMP_FILE, final_path) != 0) {
        perror("rename failed");
        unlink(TMP_FILE);

        return -1;
    }

    unlink(TMP_FILE);

    return 0;
}

int checkout(const struct repo *r, const char *branch_name) {
    // dovrà anche ripristinare la working dir
    return head_write(r, branch_name);
}