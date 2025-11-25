#include <stdio.h>
#include "../lmath.h"

typedef unsigned long long Bitboard;

typedef struct
{
    Bitboard pawn;
    Bitboard horse;
    Bitboard bishop;
    Bitboard king;
    Bitboard queen;
    Bitboard rook;
    Bitboard black;
    Bitboard white;
} BitboardSet;

void set_bit(Bitboard *bb, int sq)
{
    *bb |= (1ULL << sq);
}

char is_set(Bitboard bb, int sq)
{
    return (bb & (1ULL << sq)) != 0;
}

BitboardSet fenParse(char fenstring[255])
{
    BitboardSet bitset = {0};
    for (unsigned char i = 0; i < 255; i++)
    {
        if(!isDigit(fenstring[i])){
        
        switch (fenstring[i])
        {
        case 'P':
            set_bit(&bitset.pawn,i);
            set_bit(&bitset.white, i);
            break;
        case 'p':
            set_bit(&bitset.pawn,i);
            set_bit(&bitset.black, i);
            break;
        case 'N':
            set_bit(&bitset.horse,i);
            set_bit(&bitset.white, i);
            break;
        case 'n':
            set_bit(&bitset.horse,i);
            set_bit(&bitset.black, i);
            break;
        case 'B':
            set_bit(&bitset.bishop,i);
            set_bit(&bitset.white, i);
            break;
        case 'b':
            set_bit(&bitset.bishop,i);
            set_bit(&bitset.black, i);
            break;
        case 'R':
            set_bit(&bitset.rook,i);
            set_bit(&bitset.white, i);
            break;
        case 'r':
            set_bit(&bitset.rook,i);
            set_bit(&bitset.black, i);
            break;
        case 'Q':
            set_bit(&bitset.rook,i);
            set_bit(&bitset.white, i);
            break;
        case 'q':
            set_bit(&bitset.rook,i);
            set_bit(&bitset.black, i);
            break;
        case 'K':
            set_bit(&bitset.rook,i);
            set_bit(&bitset.white, i);
            break;
        case 'k':
            set_bit(&bitset.rook,i);
            set_bit(&bitset.black, i);
            break;
        default:

            break;
        }
        }
        else{
            if (isDigit(fenstring[i]))
            {

                i += charToInt(fenstring[i]);


                
                
            }
            
        }

    }
}