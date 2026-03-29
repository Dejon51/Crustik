#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lmath.h"
#include "zobrist.h"
#include "fen.h"

#define MAX_FEN_LEN 2000

void fenRead(Position *board, char *fen, char *arg1, char *arg2, char *arg3, char *arg4, char *arg5)
{
    memset(board, 0, sizeof(Position));
    int square = 0;

    for (int i = 0; fen[i] != '\0'; i++)
    {
        if (i >= MAX_FEN_LEN)
            break;

        board->mailbox[square] = 6;

        switch (fen[i])
        {
        case 'p':
            board->pieces[0] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square++] = 0;
            break;
        case 'n':
            board->pieces[2] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square++] = 2;
            break;
        case 'b':
            board->pieces[1] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square++] = 1;
            break;
        case 'r':
            board->pieces[3] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square++] = 3;
            break;
        case 'q':
            board->pieces[4] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square++] = 4;
            break;
        case 'k':
            board->pieces[5] |= 1ULL << square;
            board->color[1] |= 1ULL << square;
            board->mailbox[square++] = 5;
            break;

        case 'P':
            board->pieces[0] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square++] = 0;
            break;
        case 'N':
            board->pieces[2] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square++] = 2;
            break;
        case 'B':
            board->pieces[1] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square++] = 1;
            break;
        case 'R':
            board->pieces[3] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square++] = 3;
            break;
        case 'Q':
            board->pieces[4] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square++] = 4;
            break;
        case 'K':
            board->pieces[5] |= 1ULL << square;
            board->color[0] |= 1ULL << square;
            board->mailbox[square++] = 5;
            break;

        case '/':
            break;

        default:
            if (fen[i] >= '1' && fen[i] <= '8')
            {
                square += fen[i] - '0';
            }
            else
            {
                printf("Error: Invalid FEN character: %c\n", fen[i]);
                memset(board, 0, sizeof(Position));
                return;
            }
            break;
        }
    }

    // Side to move
    if (arg1[0] == 'w')
        board->turn = 0;
    else if (arg1[0] == 'b')
        board->turn = 1;
    else
    {
        printf("Error: Invalid turn in FEN: %c\n", arg1[0]);
        memset(board, 0, sizeof(Position));
        return;
    }

    // Castling rights
    board->castling = 0;
    if (arg2[0] != '-')
    {
        for (int d = 0; d < strlen(arg2); d++)
        {
            switch (arg2[d])
            {
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
                printf("Error: Invalid castling character: %c\n", arg2[d]);
                break;
            }
        }
    }

    // En passant
    if (arg3[0] != '-' && arg3[1] != '-')
    {
        int file = arg3[0] - 'a';
        int rank = arg3[1] - '1';
        board->epsquare = (file + rank * 8) & 63;
    }
    else
    {
        board->epsquare = -1;
    }

    // Half-move clock
    if (arg4 != NULL && strlen(arg4) > 0)
        board->halfmoves = atoi(arg4);
    else
        board->halfmoves = 0;

    // Full-move number
    if (arg5 != NULL && strlen(arg5) > 0)
        board->fullmoves = atoi(arg5);
    else
        board->fullmoves = 1;

    board->hash = zobrist(board);
}