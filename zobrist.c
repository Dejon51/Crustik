#include "zobrist.h"
#include "lmath.h"



uint64_t zobrist(Position *board)
{
    uint64_t hash = 0;

    for (int pt = 0; pt < 6; pt++)
    {
        uint64_t bb;
        bb = board->pieces[pt] & board->color[0];
        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            hash ^= zobrist_table[pt * 64 + sq];
            bb &= bb - 1;
        }
        bb = board->pieces[pt] & board->color[1];
        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            hash ^= zobrist_table[(pt + 6) * 64 + sq];
            bb &= bb - 1;
        }
    }

    if (board->turn)
    {
        hash ^= zobrist_table[768];
    }

    hash ^= zobrist_table[769 + (board->castling & 0xF)];

    if (board->epsquare != -1)
    {
        int file = board->epsquare & 7;
        hash ^= zobrist_table[785 + file];
    }

    return hash;
}