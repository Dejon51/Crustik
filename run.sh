#!/bin/bash

gcc main.c play.c lmath.c eval.c uci.c fen.c lmath.h search.c -g -O3 -flto -march=native "$@" -o crustik