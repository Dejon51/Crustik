#!/bin/bash

gcc main.c play.c lmath.c eval.c uci.c fen.c -g -O1 -Wall "$@" -o chess