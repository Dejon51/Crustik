@echo off

gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c -g -O3 -flto -march=native -o crustik