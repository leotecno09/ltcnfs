#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "store.h"

// #define CHECK(cond) do { \
// 	if (!(cond)) { \
// 		fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
// 		exit(1); \
// 	} \
// } while (0)


// int main(void)
// {
// 	char hash[HASH_HEX_LEN];		// hash deve essere un buffer già pronto
// 	void *content = NULL;
// 	size_t len = 0;

// 	store_init("/tmp/test");
// 	int st = store_put("hellohello\n", 11, hash);

// 	store_get(hash, &content, &len);

// 	printf("exit_code=%d, hash=%s\n", st, hash);

// 	// leggi content
// 	unsigned char *bytes = content;

// 	for (size_t i = 0; i < len; i++) {
// 		printf("%c", bytes[i]);
// 	}

// 	free(content);
// 	return 0;
// }

int main(int argc, char **argv) {
	store_init("/tmp/test");
	snapshot_dir("/tmp/test_folder");	
}