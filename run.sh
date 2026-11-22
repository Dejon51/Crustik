#!/bin/bash

gcc main.c play.c lmath.c fen.c uci.c magics.c -DNDEBUG -O3 -flto=auto -march=native -Wall -Wextra -Wshadow "$@" -o crustik
