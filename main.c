#include <stdio.h>
#include <string.h>
#include "play.h"
#include "lmath.h"
#include "uci.h"
#include "bench.h"
#include "eval.h"
#include "tt.h"

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

    uciStart();
    printf("\n");
    
    return 0;
}