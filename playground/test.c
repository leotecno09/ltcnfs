#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"

// #define CHECK(cond) do { \
// 	if (!(cond)) { \
// 		fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
// 		exit(1); \
// 	} \
// } while (0)

struct command {
	const char* name;
	int (*fn)(int argc, char **argv);
	int args;
};

int cmd_init(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return -1;
	}

	if (!store_init(argv[1])) {
		printf("Store initialized successfully in %s\n", argv[1]);
		return 0;
	}

	return -1;
}

int cmd_snapshot(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return -1;
	}

	struct snapshot_report rep;

	store_init("/tmp/test");		// da capire come mantenere a ogni comandd!!

	if (!snapshot_dir(argv[1], NULL, &rep)) {
		printf("Snapshot of %s done.\n", argv[1]);
		printf("\nREPORT\nskipped=%lu	files=%lu	dirs=%lu\n", rep.sr_skipped, rep.sr_files, rep.sr_dirs);
		return 0;
	}

	return -1;
}

static const struct command commands[] = {
	{ "init", cmd_init, 2 },		// args are "init" and "path"
	{ "snapshot", cmd_snapshot, 2 },
};

static size_t cmds_arr_size = sizeof(commands) / sizeof(struct command);

int main(int argc, char **argv) {
	if (argc == 1) {
		fprintf(stderr, "%s: please specify a command.\n", argv[0]);
		return -1;
	}
	
	struct command cmd;

	for (size_t i = 0; i < cmds_arr_size; i++) {
		cmd = commands[i];

		if (!strncmp(cmd.name, argv[1], strlen(cmd.name))) {
			if (argc - 1 != cmd.args) {
				fprintf(stderr, "%s: needs at least %d parameters.\n", argv[0], cmd.args);
				return -1;
			}

			return cmd.fn(argc - 1, argv + 1);
		}
	}
}