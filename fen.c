#include <stdio.h>
#include <string.h>
#include "lmath.h"
#include "fen.h"

#define MAX_FEN_LEN 200



void fenRead(Position *board, char *fen, char *arg1, char *arg2, char *arg3, char *arg4, char *arg5)
{
    memset(board, 0, sizeof(Position));
    int square = 0;

    // char fen[200] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    for (int i = 0; fen[i] != '\0'; i++)
    {

        if (i > MAX_FEN_LEN)
            break;
        board->mailbox[square] = 6;
        switch (fen[i])
        {
        case 'p':
            board->pieces[0] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square] = 0;
            square++;
            break;
        case 'n':
            board->pieces[2] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square] = 2;
            square++;
            break;
        case 'b':
            board->pieces[1] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square] = 1;
            square++;
            break;
        case 'r':
            board->pieces[3] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square] = 3;
            square++;
            break;
        case 'q':
            board->pieces[4] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square] = 4;
            square++;
            break;
        case 'k':
            board->pieces[5] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square] = 5;
            square++;
            break;
        case 'P':
            board->pieces[0] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square] = 0;
            square++;
            break;
        case 'N':
            board->pieces[2] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square] = 2;
            square++;
            break;
        case 'B':
            board->pieces[1] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square] = 1;
            square++;
            break;
        case 'R':
            board->pieces[3] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square] = 3;
            square++;
            break;
        case 'Q':
            board->pieces[4] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square] = 4;
            square++;
            break;
        case 'K':
            board->pieces[5] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square] = 5;
            square++;
            break;
        case '/':

            break;
        default:
            if (fen[i] >= '1' && fen[i] <= '8' || fen[i] == '\0')
            {
                square += fen[i] - '0';
            }
            else
            {
                printf("Error: Invalid fen problem: %c\n", fen[i]);
                memset(board, 0, sizeof(Position));
                break;
            }
            break;
        }
    }

    if (arg1[0] == 'w')
    {
        board->turn = 0;
    }
    else if (arg1[0] == 'b')
    {
        board->turn = 1;
    }
    else
    {
        printf("Error: Invalid fen problem: %c\n", arg1[0]);
        memset(board, 0, sizeof(Position));
    }
    int lengthcastling = strlen(arg2);
    board->castling = 0;

if (arg2[0] != '-') {
    int lengthcastling = strlen(arg2);
    for (int d = 0; d < lengthcastling; d++) {
        switch (arg2[d]) {
        case 'K':
            board->castling |= (1U << WHITE_KINGSIDE);
            break;
        case 'Q':
            board->castling |= (1U << WHITE_QUEENSIDE);
            break;
        case 'k':
            board->castling |= (1U << BLACK_KINGSIDE);
            break;
        case 'q':
            board->castling |= (1U << BLACK_QUEENSIDE);
            break;
        default:
            printf("Error: Invalid castling character in FEN: %c\n", arg2[d]);
            break;
        }
    }
} else {
    board->castling = 0; // no castling rights
}

    if (arg3[0] != '-' && arg3[1] != '-')
    {
        int file = arg3[0] - 'a';
        int rank = arg3[1] - '1'; // <- note '1', not '0'
        board->epsquare = (file + rank * 8) & 63;
    }
}