#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "uci.h"
#include "lmath.h"
#include "eval.h"


int main()
{
    // MoveList* list = {0};
    // legalMoveGen(list);
    init_tables();
    uciStart();
    // makeMove(board,0);// 0 white black 1
    printf("\n");

    return 1;
}
