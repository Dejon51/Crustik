@echo off

gcc main.c play.c lmath.c eval.c uci.c fen.c search.c -g -O1 -march=native -o chess