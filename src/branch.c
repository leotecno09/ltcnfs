#include "branch.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

int new_branch(const char* store_root, const char *name, const char *last_commit) {
    char TMP_FILE[] = "tmpbranch_XXXXXX";

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
    int n = snprintf(final_path, sizeof final_path, "%s/refs/branches/%s", store_root, name);

    /* check if the path was truncated */
    if (n < 0 || (size_t)n >= sizeof final_path)
        return -1;

    if (rename(TMP_FILE, final_path) != 0) {
        perror("rename failed");            // si blocca qua con no such file or directory
        unlink(TMP_FILE);

        return -1;
    }

    unlink(TMP_FILE);

    // should we call checkout()?

    return 0;
}