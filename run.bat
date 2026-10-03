@echo off
setlocal

if "%~1"=="" (
    set EVALFILE=quant384hl.bin
) else (
    set EVALFILE=%~1
)

gcc main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c tt.c bench.c text.c magics.c params.c ordermoves.c -lm -DNDEBUG -O3 -flto=auto -march=native -Wall -Wextra -Wshadow -DEVALFILE=\"%EVALFILE%\" -o crustik

endlocal
