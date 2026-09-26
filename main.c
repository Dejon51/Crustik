#include "bench.h"
#include "eval.h"
#include "lmath.h"
#include "magics.h"
#include "play.h"
#include "tt.h"
#include "uci.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	initMagics();
	init_tables();

	if (argc > 1 && strcmp(argv[1], "bench") == 0) {
		bench();
		return 0;
	} else if (argc > 1 && strcmp(argv[1], "movegen") == 0) {
		bench_movegen();
		return 0;
	} else if (argc > 1 && strncmp(argv[1], "genfens", 7) == 0 &&
			   (argv[1][7] == '\0' || argv[1][7] == ' ' ||
				argv[1][7] == '\t')) {
		genfensRun(argc, argv);
		return 0;
	}

	uciStart();
	printf("\n");
	return 0;
}
