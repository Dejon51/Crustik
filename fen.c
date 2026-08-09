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

    for (int i = 0; i < 64; i++)
    {
        board->mailbox[i] = 6;
    }

    int square = 0;

    for (int i = 0; fen[i] != '\0'; i++)
    {
        if (i >= MAX_FEN_LEN || square >= 64)
            break;

        char c = fen[i];

        if (c == '/')
            continue;

        if (c >= '1' && c <= '8')
        {
            square += (c - '0');
            continue;
        }

        switch (c)
        {
        // Black pieces (color index 1)
        case 'p':
            board->pieces[0] |= (1ULL << square);
            board->color[1]  |= (1ULL << square);
            board->mailbox[square++] = 0;
            break;
        case 'b':
            board->pieces[1] |= (1ULL << square);
            board->color[1]  |= (1ULL << square);
            board->mailbox[square++] = 1;
            break;
        case 'n':
            board->pieces[2] |= (1ULL << square);
            board->color[1]  |= (1ULL << square);
            board->mailbox[square++] = 2;
            break;
        case 'r':
            board->pieces[3] |= (1ULL << square);
            board->color[1]  |= (1ULL << square);
            board->mailbox[square++] = 3;
            break;
        case 'q':
            board->pieces[4] |= (1ULL << square);
            board->color[1]  |= (1ULL << square);
            board->mailbox[square++] = 4;
            break;
        case 'k':
            board->pieces[5] |= (1ULL << square);
            board->color[1]  |= (1ULL << square);
            board->mailbox[square++] = 5;
            break;

        case 'P':
            board->pieces[0] |= (1ULL << square);
            board->color[0]  |= (1ULL << square);
            board->mailbox[square++] = 0;
            break;
        case 'B':
            board->pieces[1] |= (1ULL << square);
            board->color[0]  |= (1ULL << square);
            board->mailbox[square++] = 1;
            break;
        case 'N':
            board->pieces[2] |= (1ULL << square);
            board->color[0]  |= (1ULL << square);
            board->mailbox[square++] = 2;
            break;
        case 'R':
            board->pieces[3] |= (1ULL << square);
            board->color[0]  |= (1ULL << square);
            board->mailbox[square++] = 3;
            break;
        case 'Q':
            board->pieces[4] |= (1ULL << square);
            board->color[0]  |= (1ULL << square);
            board->mailbox[square++] = 4;
            break;
        case 'K':
            board->pieces[5] |= (1ULL << square);
            board->color[0]  |= (1ULL << square);
            board->mailbox[square++] = 5;
            break;

        default:
            printf("Error: Invalid FEN character: %c\n", c);
            memset(board, 0, sizeof(Position));
            return;
        }
    }

    board->color[2] = board->color[0] | board->color[1];

    if (arg1 != NULL && arg1[0] == 'w')
        board->turn = 0;
    else if (arg1 != NULL && arg1[0] == 'b')
        board->turn = 1;
    else
    {
        printf("Error: Invalid turn in FEN: %s\n", arg1 ? arg1 : "NULL");
        memset(board, 0, sizeof(Position));
        return;
    }

    board->castling = 0;
    if (arg2 != NULL && arg2[0] != '-')
    {
        for (size_t d = 0; d < strlen(arg2); d++)
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

    if (arg3 != NULL && arg3[0] != '-' && strlen(arg3) >= 2)
    {
        int file = arg3[0] - 'a';
        int rank_row = '8' - arg3[1];
        board->epsquare = (rank_row * 8 + file) & 63;
    }
    else
    {
        board->epsquare = -1;
    }

    if (arg4 != NULL && strlen(arg4) > 0)
        board->halfmoves = atoi(arg4);
    else
        board->halfmoves = 0;

    if (arg5 != NULL && strlen(arg5) > 0)
        board->fullmoves = atoi(arg5);
    else
        board->fullmoves = 1;

    board->hash = zobrist(board);
}