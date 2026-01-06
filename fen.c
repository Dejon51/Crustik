#include <stdio.h>
#include <string.h>
#include "lmath.h"
#include "fen.h"

#define MAX_FEN_LEN 200

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

BitboardSet fenRead(char *fen)
{
    bool invalidfen = 0;
    BitboardSet board = {0};
    int square = 0;
    // char fen[200] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    for (int i = 0; fen[i] != ' '; i++)
    {
        if (i > MAX_FEN_LEN)
            break;

        switch (fen[i])
        {
        case 'p':
            board.pieces[0] = set_bit(board.pieces[0], square, 1);
            board.color[1] = set_bit(board.color[1], square, 1);
            square++;
            break;
        case 'n':
            board.pieces[2] = set_bit(board.pieces[2], square, 1);
            board.color[1] = set_bit(board.color[1], square, 1);
            square++;
            break;
        case 'b':
            board.pieces[1] = set_bit(board.pieces[1], square, 1);
            board.color[1] = set_bit(board.color[1], square, 1);
            square++;
            break;
        case 'r':
            board.pieces[3] = set_bit(board.pieces[3], square, 1);
            board.color[1] = set_bit(board.color[1], square, 1);
            square++;
            break;
        case 'q':
            board.pieces[4] = set_bit(board.pieces[4], square, 1);
            board.color[1] = set_bit(board.color[1], square, 1);
            square++;
            break;
        case 'k':
            board.pieces[5] = set_bit(board.pieces[5], square, 1);
            board.color[1] = set_bit(board.color[1], square, 1);
            square++;
            break;
        case 'P':
            board.pieces[0] = set_bit(board.pieces[0], square, 1);
            board.color[0] = set_bit(board.color[0], square, 1);
            square++;
            break;
        case 'N':
            board.pieces[2] = set_bit(board.pieces[2], square, 1);
            board.color[0] = set_bit(board.color[0], square, 1);
            square++;
            break;
        case 'B':
            board.pieces[1] = set_bit(board.pieces[1], square, 1);
            board.color[0] = set_bit(board.color[0], square, 1);
            square++;
            break;
        case 'R':
            board.pieces[3] = set_bit(board.pieces[3], square, 1);
            board.color[0] = set_bit(board.color[0], square, 1);
            square++;
            break;
        case 'Q':
            board.pieces[4] = set_bit(board.pieces[4], square, 1);
            board.color[0] = set_bit(board.color[0], square, 1);
            square++; 
            break;
        case 'K':
            board.pieces[5] = set_bit(board.pieces[5], square, 1);
            board.color[0] = set_bit(board.color[0], square, 1);
            square++;
            break;
        case '/':

            break;
        default:
            if (fen[i] >= '1' && fen[i] <= '8')
                square += fen[i] - '0';
            else{
                memset(&board, 0, sizeof(BitboardSet));
                return board;
                break;
            }
            break;
        }
    }
    print_binary(board.pieces[0]);

    return board;
}