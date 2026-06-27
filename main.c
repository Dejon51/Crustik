#include <stdio.h>
#include <string.h>
#include "play.h"
#include "lmath.h"
#include "uci.h"
#include "bench.h"
#include "eval.h"
#include <stdlib.h>
#include "datagen.h"

int main(int argc, char **argv)
{
    init_tables();

    if (argc > 1 && strcmp(argv[1], "bench") == 0)
    {
        bench();
        return 0;
    }
    else if (argc > 1 && strcmp(argv[1], "movegen") == 0){
        bench_movegen();
        return 0;
    }
    else if (argc >= 2 && strcmp(argv[1], "datagen") == 0) {
        uint64_t seed = 1234;
        uint16_t fens = 10000;
        const char *bookfile = "None";

        if (argc >= 3)
            seed = strtoull(argv[2], NULL, 10);

        if (argc >= 4)
            fens = (uint16_t)atoi(argv[3]);

        if (argc >= 5)
            bookfile = argv[4];

        genfens(seed, fens, bookfile);
        return 0;
    }

    uciStart();

    printf("\n");
    return 0;
}