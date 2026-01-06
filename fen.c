#include <stdio.h>
#include "lmath.h"
#include "fen.h"

void print_binary(uint64_t bb)
{
    for (int i = 63; i >= 0; i--)
    {
        printf("%c", (bb & (1ULL << i)) ? '1' : '0');
        if (i % 8 == 0)
            printf(" ");
    }
    printf("\n");
}

BitboardSet fenRead(char *inputfen)
{
    bool invalidfen = 0;
    BitboardSet board = {0};
    // char inputfen[200] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    for (unsigned char i = 0; i < 20; i++)
    {
        switch (inputfen[i])
        {
        case 'p':
            board.pieces[0] = set_bit(board.pieces[0], i, 1);
            board.color[1] = set_bit(board.color[1], i, 1);
            break;
        case 'n':
            board.pieces[2] = set_bit(board.pieces[2], i, 1);
            board.color[1] = set_bit(board.color[1], i, 1);
            break;
        case 'b':
            board.pieces[1] = set_bit(board.pieces[1], i, 1);
            board.color[1] = set_bit(board.color[1], i, 1);
            break;
        case 'r':
            board.pieces[3] = set_bit(board.pieces[3], i, 1);
            board.color[1] = set_bit(board.color[1], i, 1);
            break;
        case 'q':
            board.pieces[4] = set_bit(board.pieces[4], i, 1);
            board.color[1] = set_bit(board.color[1], i, 1);
            break;
        case 'k':
            board.pieces[5] = set_bit(board.pieces[5], i, 1);
            board.color[1] = set_bit(board.color[1], i, 1);
            break;
        case 'P':
            board.pieces[0] = set_bit(board.pieces[0], i, 1);
            board.color[0] = set_bit(board.color[0], i, 1);
            break;
        case 'N':
            board.pieces[2] = set_bit(board.pieces[2], i, 1);
            board.color[0] = set_bit(board.color[0], i, 1);
            break;
        case 'B':
            board.pieces[1] = set_bit(board.pieces[1], i, 1);
            board.color[0] = set_bit(board.color[0], i, 1);
            break;
        case 'R':
            board.pieces[3] = set_bit(board.pieces[3], i, 1);
            board.color[0] = set_bit(board.color[0], i, 1);
            break;
        case 'Q':
            board.pieces[4] = set_bit(board.pieces[4], i, 1);
            board.color[0] = set_bit(board.color[0], i, 1);
            break;
        case 'K':
            board.pieces[5] = set_bit(board.pieces[5], i, 1);
            board.color[0] = set_bit(board.color[0], i, 1);
            break;
        default:
            if (!(inputfen[i] >= '0' && inputfen[i] <= '9') && inputfen[i] != '/')
            {
                i=20;
                printf("Error: Invalid Fen\n");
            }
            
            break;
        }
    }
    print_binary(board.pieces[0]);
    if (!invalidfen)
    {
        return board;  
    }
}