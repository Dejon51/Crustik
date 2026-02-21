#!/bin/bash

gcc main.c play.c lmath.c eval.c uci.c fen.c lmath.h -g -O1 -march=native "$@" -o chess