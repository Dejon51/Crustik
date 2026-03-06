#include <stdint.h>
#include <stdio.h>

typedef uint64_t Bitboard;

// --- KING STUFF ---

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
            continue;

        kingmask |= (1ULL << (nx + ny * 8));
    }
    return kingmask;
}

// --- PAWN ATTACK STUFF (1D ARRAYS) ---

Bitboard pawnAttacks(int color, int ind)
{
    Bitboard mask = 0ULL;
    int x = ind % 8;
    int y = ind / 8;

    if (color == 0) // white pawns attack upward
    {
        if (x > 0 && y < 7) mask |= (1ULL << ((y + 1) * 8 + (x - 1)));
        if (x < 7 && y < 7) mask |= (1ULL << ((y + 1) * 8 + (x + 1)));
    }
    else // black pawns attack downward
    {
        if (x > 0 && y > 0) mask |= (1ULL << ((y - 1) * 8 + (x - 1)));
        if (x < 7 && y > 0) mask |= (1ULL << ((y - 1) * 8 + (x + 1)));
    }

    return mask;
}

// --- GENERATE BLOCKERS.H ---

void genblockers()
{
    FILE *fptr = fopen("blockers.h", "w");
    if (!fptr)
    {
        perror("Failed to open file");
        return;
    }

    fprintf(fptr, "#ifndef BLOCKERS_H\n#define BLOCKERS_H\n\n");

    // // KING TABLE
    // fprintf(fptr, "uint64_t kingtable[64] = {\n");
    // for (int i = 0; i < 64; i++)
    // {
    //     Bitboard moves = kingMask(i);
    //     fprintf(fptr, "0x%016llxULL%s\n", moves, (i < 63) ? "," : "");
    // }
    // fprintf(fptr, "};\n\n");

    // 1D PAWN ATTACK TABLES
    fprintf(fptr, "uint64_t white_pawn_attacks[64] = {\n");
    for (int i = 0; i < 64; i++)
    {
        fprintf(fptr, "0x%016llxULL%s\n", pawnAttacks(0, i), (i < 63) ? "," : "");
    }
    fprintf(fptr, "};\n\n");

    fprintf(fptr, "uint64_t black_pawn_attacks[64] = {\n");
    for (int i = 0; i < 64; i++)
    {
        fprintf(fptr, "0x%016llxULL%s\n", pawnAttacks(1, i), (i < 63) ? "," : "");
    }
    fprintf(fptr, "};\n\n");

    fprintf(fptr, "#endif // BLOCKERS_H\n");

    fclose(fptr);
}

int main()
{
    genblockers();
    return 0;
}