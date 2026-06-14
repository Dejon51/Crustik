@echo off

gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c bench.c text.c -DNDEBUG -O3 -flto -march=native -Wall -Wextra -Wshadow  -o crustik