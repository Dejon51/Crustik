#!/bin/bash

gcc main.c play.c lmath.c eval.c uci.c fen.c -s -g "$@" -o chess