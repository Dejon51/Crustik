#include <stdio.h>
#include "text.h"

const char *HELP_TEXT =

"uci              -> init engine\n"
"isready          -> ready check\n"
"ucinewgame       -> reset game state\n"
"quit             -> exit engine\n"
"\n"
"position startpos [moves ...]\n"
"position fen <fen> [moves ...]\n"
"                 -> set board position\n"
"\n"
"go nodes <n>     -> search fixed nodes\n"
"go depth <n>     -> search fixed depth\n"
"go movetime <ms> -> search fixed time\n"
"go perft <n>     -> perft test\n"
"go wtime/btime/winc/binc\n"
"                 -> timed search\n"
"\n"
"d                -> display board\n"
"pml <i>          -> test move list entry\n";