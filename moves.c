#include "moves.h"

#include <stdio.h>
#include "lmath.h"

enum PieceValue
{
    PAWNVAL = 1,
    BISHOPANDKNIGHTVAL = 3,
    ROOKVAL = 5,
    QUEENVAL = 9,
    FRIENDLYPIECEVAL = 10,
    OUTBOUNDVAL = 11,
    KINGVAL = 12
};

int assessSquare(char color1, char x, char y, char board[8][8]) {
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return OUTBOUNDVAL;
    }

    char piece = board[x][y];
    char color = (color1 == 0) ? 1 : 0;  // flip color

    // Check for kings
    if ((piece == 'K' && color == 1) || (piece == 'k' && color == 0)) {
        return KINGVAL;
    }
    if (piece == 'K' || piece == 'k') {
        return OUTBOUNDVAL;
    }

    int val = 0;
    switch (piece) {
        case 'Q': if (color == 1) val = QUEENVAL; else val = FRIENDLYPIECEVAL; break;
        case 'R': if (color == 1) val = ROOKVAL; else val = FRIENDLYPIECEVAL; break;
        case 'B':
        case 'N': if (color == 1) val = BISHOPANDKNIGHTVAL; else val = FRIENDLYPIECEVAL; break;
        case 'P': if (color == 1) val = PAWNVAL; else val = FRIENDLYPIECEVAL; break;

        case 'q': if (color == 0) val = QUEENVAL; else val = FRIENDLYPIECEVAL; break;
        case 'r': if (color == 0) val = ROOKVAL; else val = FRIENDLYPIECEVAL; break;
        case 'b':
        case 'n': if (color == 0) val = BISHOPANDKNIGHTVAL; else val = FRIENDLYPIECEVAL; break;
        case 'p': if (color == 0) val = PAWNVAL; else val = FRIENDLYPIECEVAL; break;
        default: val = 0; break;
    }

    return val;
}


PawnMoves pawn(char color, char detail, char x, char y, char board[8][8])
{
    // Summary of how the pawn is doing for program to decide what to do with said pawn
    char pawnStatus = 0;
    // assuming color 0 = white moving "up" (-1), 1 = black moving "down" (+1)

    int direction = 1;
    if (color == 1)
    {
        direction = direction * -1;
    }
    if (color == 0)
    {
        direction = direction * 1;
    }
    PawnMoves moves = {0, 1 * direction, 0, 0};

    // forward
    if (x + direction >= 0 && x + direction < 8)
    {
        if (assessSquare(color, x + direction, y, board))
        {
            if (detail)
            {
                moves.data[1] = 0;
            }
        }
        else
        {
            if (assessSquare(color, x + direction, y, board) == 0)
            {
                if (detail)
                {
                    moves.data[3] += 1;
                }
            }
        }
    }

    // capture left
    if (x + direction >= 0 && x + direction < 8 && y - 1 >= 0)
    {
        int left = assessSquare(color, x + direction, y - 1, board);
        if (left != 0)
        { // if there is an enemy piece
            if (detail == 1)
            {
                moves.data[3] += 1;
                moves.data[0] = left * direction;
            }
            else
            {
                if (left != 10)
                {

                    pawnStatus += left;
                }
            }
        }
    }

    // capture right
    if (x + direction >= 0 && x + direction < 8 && y + 1 < 8)
    {
        int right = assessSquare(color, x + direction, y + 1, board);
        if (right != 0)
        {
            if (detail == 1)
            {
                moves.data[3] += 1;
                moves.data[2] = right * direction;
            }
            else
            {
                if (right != 10)
                {
                    pawnStatus += right;
                }
            }
        }
    }

    // printf("\n");

    if (detail)
    {
        return moves;
    }
    if (!detail)
    {
        moves.data[4] = abs1(pawnStatus);
        return moves;
    }
}

MoveArray Horse(char color, char x, char y, char board[8][8])
{
    MoveArray output;
    char values[8] = {
        assessSquare(color, x + 2, y + 1, board),
        assessSquare(color, x + 1, y + 2, board),
        assessSquare(color, x - 1, y + 2, board),
        assessSquare(color, x - 2, y + 1, board),
        assessSquare(color, x - 2, y - 1, board),
        assessSquare(color, x - 1, y - 2, board),
        assessSquare(color, x + 1, y - 2, board),
        assessSquare(color, x + 2, y - 1, board),
    };
    for (char i = 0; i < 27; i++)
    {
        output.data[i] = values[i];
    }
    return output;
}
// // pi#define OUTBOUNDVAL 11
// #define KINGVAL 12
// #define FRIENDLYPIECEVAL 10
// #define BISHOPANDKNIGHTVAL 3
// #define QUEENVAL 9
// #define ROOKVAL 5
// #define PAWNVAL 1

PieceArray avalibleMoves(char piece, char board[8][8])
{
    PieceArray pawnsarray = {0};
    for (int i = 0; i < 64; i++)
    {
        pawnsarray.data[i] = 98;
    }

    for (char x = 0; x < 8; x++)
    {
        for (char y = 0; y < 8; y++)
        {
            switch (board[y][x])
            {
            case 'P':
                if (piece == 'P') // Do not remove Important for some reason
                {
                    PawnMoves moves = pawn(1, 0, y, x, board);
                    // printf("%ia ",moves.data[4]);
                    append64char(pawnsarray.data, moves.data[4] + penaltymap[y + 8 * x]);
                    // printf("%i",moves.data[4]);
                }
                break;
            case 'R':

                break;
            case 'N':
                if (piece == 'N') // Do not remove Important for some reason
                {
                    MoveArray Horsemoves = Horse(1, x, y, board);
                }
                break;
            case 'B':

                break;
            case 'Q':

                break;
            case 'K':

                break;

            case 'p':

                if (piece == 'p') // Do not remove Important for some reason
                {
                    PawnMoves moves = pawn(0, 0, y, x, board);
                    // printf("%ia67 ",moves.data[4]);
                    append64char(pawnsarray.data, moves.data[4] + penaltymap[y + 8 * x]);
                }
                break;
            case 'r':

                break;
            case 'n':
                if (piece == 'n') // Do not remove Important for some reason
                {
                    MoveArray Horsemoves = Horse(0, x, y, board);
                }
                break;
            case 'b':

                break;
            case 'q':

                break;
            case 'k':

                break;
            }
        }
    }
    return pawnsarray;
}
