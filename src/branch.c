// dovremmo usare più goto e int retval

#include "repo.h"
#include "branch.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

static int head_write(const struct repo *r, const char *new_branch) {
    char HEAD_TMP[PATH_MAX];
    char BRANCH_LOCATION[PATH_MAX + 2];

    repo_path(r, BRANCH_LOCATION, sizeof BRANCH_LOCATION, "refs/branches/%s", new_branch);
    printf("[DEBUG] BRANCH_LOCATION=%s\n", BRANCH_LOCATION);

    struct stat st;
    if (stat(BRANCH_LOCATION, &st) == -1) {
        perror("branch does not exist");
        return -1;       // magari distinguere per errore "umano"?
    }

    repo_path(r, HEAD_TMP, sizeof HEAD_TMP, "TMPHEAD_XXXXXX");
    printf("[DEBUG] HEAD_TMP=%s\n", HEAD_TMP);
    int fd = mkstemp(HEAD_TMP);

    if (fd == -1) {
        perror("mkstemp failed");
        return -1;
    }

    // add the '\n' character to the BRANCH_LOCATION
    int n = repo_path(r, BRANCH_LOCATION, sizeof BRANCH_LOCATION, "refs/branches/%s\n", new_branch);

    if (write_all(fd, BRANCH_LOCATION, (size_t)n) == -1) {
        perror("write failed on temp branch");
        close(fd);
        unlink(HEAD_TMP);
        return -1;
    }

    close(fd);

    char HEAD_LOCATION[PATH_MAX];
    repo_path(r, HEAD_LOCATION, sizeof HEAD_LOCATION, "%s", "HEAD");
    printf("[DEBUG] HEAD_LOCATION=%s\n", HEAD_LOCATION);

    if (rename(HEAD_TMP, HEAD_LOCATION) != 0) {
        perror("renaming HEAD");
        unlink(HEAD_TMP);

        return -1;
    }

    unlink(HEAD_TMP);

    return 0;
}

int branch_update(const struct repo *r, const char *branch_name, const char commit[HASH_HEX_LEN]) {
    // points the branch to the last commit
    char branch_path[PATH_MAX];
    char temp_branch_path[PATH_MAX];
    repo_path(r, branch_path, sizeof branch_path, "refs/branches/%s", branch_name);
    repo_path(r, temp_branch_path, sizeof temp_branch_path, "refs/branches/tmp%s_XXXXXX", branch_name);

    printf("[DEBUG] branch_path=%s\n", branch_path);
    printf("[DEBUG] temp_branch_path=%s\n", temp_branch_path);

    struct stat st;
    if (stat(branch_path, &st) == -1) {
        perror("branch does not exist");
        return -1;       // magari distinguere per errore "umano"?
    }

    int tmpfd = mkstemp(temp_branch_path);
    if (tmpfd == -1) {
        perror("mkstemp failed");
        return -1;
    }

    // boh (da rivedere, ottimizzare, sistemare, fare, disfare, ciao)
    char line[HASH_HEX_LEN + 1];
    snprintf(line, sizeof line, "%s\n", commit);

    if (write_all(tmpfd, line, HASH_HEX_LEN + 1) == -1) {     // forse il '\0' in commit[] rompe
        perror("write_all failed");
        close(tmpfd);
        unlink(temp_branch_path);

        return -1;
    }

    close(tmpfd);

    if (rename(temp_branch_path, branch_path) != 0) {
        perror("rename failed");
        unlink(temp_branch_path);

        return -1;
    }

    unlink(temp_branch_path);

    return 0;
}

int new_branch(const struct repo *r, const char *name, const char last_commit[HASH_HEX_LEN]) {
    char TMP_FILE[PATH_MAX];

    snprintf(TMP_FILE, sizeof TMP_FILE, "%s/refs/branches/tmpbranch_XXXXXX", r->root);   // ci fidiamo di sta snprintf per ora

    int branchf = mkstemp(TMP_FILE);
    if (branchf == -1) {
        perror("mkstemp failed");
        return -1;
    }

    if (last_commit != NULL) {
        if (write_all(branchf, last_commit, HASH_HEX_LEN - 1) == -1) {
            perror("write_all failed");
            close(branchf);
            unlink(TMP_FILE);

            return -1;
        }
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
    //memcpy(r->current_branch, branch_name, sizeof r->current_branch);
    snprintf(r->current_branch, MAX_BRANCH_NAME_LEN, "%s", branch_name);
    return head_write(r, r->current_branch);        // così è un po' inutile il secondo parametro ma vb
}