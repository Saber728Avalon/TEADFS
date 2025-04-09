#include "test.h"
#include <stdio.h>
#include "lib_tead_fs.h"

int main(int argc, char *argv[]) {
	printf("start test\n");

	StartTEADFS();

	while (1){
		getchar();
	}
}