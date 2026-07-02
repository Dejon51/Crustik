#!/bin/bash

x86_64-w64-mingw32-gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c datagen.c -mno-avx -mno-avx2 -mno-fma -DNDEBUG -O3 -flto -Wall -Wextra -Wshadow -static -o crustik-0.2.0-win64.exe -lwinpthread
x86_64-w64-mingw32-gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c datagen.c -mavx2 -march=native -DNDEBUG -O3 -flto -Wall -Wextra -Wshadow -static -o crustik-0.2.0-win64-avx2.exe -lwinpthread


gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c datagen.c -DNDEBUG -O3 -flto -march=native -Wall -Wextra -static -Wshadow "$@" -o crustik-0.2.0-linux64-avx2
gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c datagen.c -DNDEBUG -O3 -flto -mno-avx -mno-avx2 -mno-fma -Wall -static -Wextra -Wshadow "$@" -o crustik-0.2.0-linux64