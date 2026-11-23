#ifndef _LTCNFS_BRANCH
#define _LTCNFS_BRANCH

/*
    creates a new branch under refs/branches
    the branch will point to the last commit of the working dir
    0 = ok, -1 = error
*/
int new_branch(const char* store_root, const char* name, const char *last_commit);

/*
    change where the HEAD points. Now it will point to branch_hash
    0 = ok, -1 = error
*/
int checkout(const char *branch_hash);

#endif