@echo off

gcc main.c play.c lmath.c eval.c uci.c fen.c -g -O1 "$@" -o chess