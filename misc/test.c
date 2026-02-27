#include "../lmath.h"
#include <stdint.h>
#include <stdio.h>

Bitboard kingMask(int ind)
{
    Bitboard kingmask = 0ULL;

    int x = ind % 8;
    int y = ind / 8;

    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; i++)
    {
        int nx = x + dx[i];
        int ny = y + dy[i];

        if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
        {
            continue;
        }
        else
        {
            kingmask |= (1ULL << (nx + ny * 8));
        }
    }
    return kingmask;
}

int genblockers()
{
    FILE *fptr;
    fptr = fopen("blockers.h", "w");
    fprintf(fptr, "uint64_t kingtable[] = {\n");

    for (int i = 0; i < 64; i++)
    {
            uint64_t moves = kingMask(i);
            fprintf(fptr, "0x%llxULL,\n", moves);
    }
    fprintf(fptr, "};\n");

    fclose(fptr);
}

int main()
{
    genblockers();
}