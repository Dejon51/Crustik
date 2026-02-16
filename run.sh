#!/bin/bash

gcc main.c play.c lmath.c eval.c uci.c fen.c -g -flto -O1 "$@" -o chess