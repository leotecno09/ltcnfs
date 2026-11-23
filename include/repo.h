#ifndef _LTCNFS_REPO
#define _LTCNFS_REPO

#include <limits.h>
#include "store.h"

struct repo {
    char root[PATH_MAX];
    char current_branch[21];    // branch name cannot be more than 20 characters
    char last_commit[HASH_HEX_LEN];
};

/*
    initializes the repository, the caller has to allocate a repo struct
    and give the root path of the repo to the function
    0 = ok, -1 = error
*/
// int repo_open(struct repo *r, const char *root);

#endif