#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"
#include "branch.h"
#include "repo.h"

// #define CHECK(cond) do { \
// 	if (!(cond)) { \
// 		fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
// 		exit(1); \
// 	} \
// } while (0)

#define MAX_ARGS 16

struct command {
	const char* name;
	int (*fn)(int argc, char **argv, struct repo *r);
	int args;
};

char latest_tree[HASH_HEX_LEN]; // aiuto nn credo sia la cosa migliore

static int cmd_init(int argc, char **argv, struct repo *r) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return -1;
	}

	if (!store_init(r, argv[1])) {
		printf("Store initialized successfully in %s\n", argv[1]);
		return 0;
	}

	return -1;
}

static int cmd_new_branch(int argc, char **argv, struct repo *r) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <branch_name>", argv[0]);
		return -1;
	}

	if (store_init(r, "/tmp/test") == -1) {
		perror("store init failed");
		return -1;
	}

	if (!new_branch(r, argv[1], r->last_commit)) {
		printf("Branch %s was created successfully\n", argv[1]);
		return 0;
	}

	return -1;
}

static int cmd_snapshot(int argc, char **argv, struct repo *r) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return -1;
	}

	struct snapshot_report rep;

	//store_init(r, "/tmp/test");		// da capire come mantenere a ogni comandd!!

	if (!snapshot_dir(r, argv[1], latest_tree, &rep)) {
		printf("Snapshot of %s done.\n", argv[1]);
		printf("\nREPORT\nskipped=%lu	files=%lu	dirs=%lu\n", rep.sr_skipped, rep.sr_files, rep.sr_dirs);
		return 0;
	}

	return -1;
}

static int cmd_commit(int argc, char **argv, struct repo *r) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return -1;
	}

	if (!commit(r, latest_tree, argv[1])) {
		printf("Commit done.\n");
		return 0;
	}

	return -1;
}

static const struct command commands[] = {
	{ "init", cmd_init, 2 },		// args are "init" and "path"
	{ "snapshot", cmd_snapshot, 2 },
	{ "branch", cmd_new_branch, 2 },
	{ "commit", cmd_commit, 2 }
};

static size_t cmds_arr_size = sizeof(commands) / sizeof(struct command);

// int main(int argc, char **argv) {
// 	if (argc == 1) {
// 		fprintf(stderr, "%s: please specify a command.\n", argv[0]);
// 		return -1;
// 	}
	
// 	struct command cmd;
// 	struct repo r;

// 	for (size_t i = 0; i < cmds_arr_size; i++) {
// 		cmd = commands[i];

// 		if (!strncmp(cmd.name, argv[1], strlen(cmd.name))) {
// 			if (argc - 1 != cmd.args) {
// 				fprintf(stderr, "%s: needs at least %d parameters.\n", argv[0], cmd.args);
// 				return -1;
// 			}

// 			return cmd.fn(argc - 1, argv + 1, &r);
// 		}
// 	}
// }

static int split_args(char *line, char *argv[], int max) {
	int argc = 0;
	char *tok = strtok(line, " \t\n");

	while (tok != NULL && argc < max - 1) {
		argv[argc++] = tok;
		tok = strtok(NULL, " \t\n");
	}

	argv[argc] = NULL;
	return argc;
}

int main(void) {
	struct command cmd;
	struct repo r;

	char line[1024];
	char *args[MAX_ARGS];

	while (1) {
		printf("\n\n[REPOSITORY]\n\nroot=%s\ncurrent_branch=%s\nlast_commit=%s\n\n", r.root, r.current_branch, r.last_commit);
		printf("ltcnfs> ");
		fflush(stdout);

		if (fgets(line, sizeof line, stdin) == NULL) break;

		int argc = split_args(line, args, MAX_ARGS);
		if (argc == 0) continue;
		
		if (!strncmp(args[0], "exit", sizeof args[0])) break;

		for (size_t i = 0; i < cmds_arr_size; i++) {
			cmd = commands[i];

			if (!strncmp(cmd.name, args[0], strlen(cmd.name))) {
				cmd.fn(argc, args, &r);
			}
		}
	}
}