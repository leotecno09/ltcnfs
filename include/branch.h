#ifndef _LTCNFS_BRANCH
#define _LTCNFS_BRANCH

#include "store.h"

#define MAX_BRANCH_NAME_LEN 20

/*
    creates a new branch under refs/branches
    the branch will point to the last commit of the working dir
    0 = ok, -1 = error
*/
int new_branch(const struct repo *r, const char* name, const char last_commit[HASH_HEX_LEN]);

/*
    update the branch and change the ref with the last commit hash
*/
int branch_update(const struct repo *r, const char *branch_name, const char commit[HASH_HEX_LEN]);

/*
    change where the HEAD points and reset the working dir to the new branch
    0 = ok, -1 = error
*/
int checkout(const struct repo *r, const char *branch_name);

#endif