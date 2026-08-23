#!/bin/bash

EVALFILE="${EVALFILE:-quant128hl.bin}"

gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c datagen.c -lm -DNDEBUG -O3 -flto -march=native -Wall -Wextra -Wshadow -DEVALFILE=\"$EVALFILE\" "$@" -o crustik
