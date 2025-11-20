#include "eval.h"

short threatAccess(char board[8][8])
{
    for (char x = 0; x < 8; x++)
    {
        for (char y = 0; y < 8; y++)
        {
        }
    }
}

short eval(char board[8][8])
{
    short points = 0;

    char whitePieces[6] = {0, 0, 0, 0, 0, 0};
    char blackPieces[6] = {0, 0, 0, 0, 0, 0};
    // Order Is Pawn, Rook, Knight, Bishop, Queen, King

    for (char x = 0; x < 8; x++)
    {
        for (char y = 0; y < 8; y++)
        {
            switch (board[y][x])
            {
            case 'P':
                whitePieces[0]++;
                break;
            case 'R':
                whitePieces[1]++;
                break;
            case 'N':
                whitePieces[2]++;
                break;
            case 'B':
                whitePieces[3]++;
                break;
            case 'Q':
                whitePieces[4]++;
                break;
            case 'K':
                whitePieces[5]++;
                break;

            case 'p':
                blackPieces[0]++;
                break;
            case 'r':
                blackPieces[1]++;
                break;
            case 'n':
                blackPieces[2]++;
                break;
            case 'b':
                blackPieces[3]++;
                break;
            case 'q':
                blackPieces[4]++;
                break;
            case 'k':
                blackPieces[5]++;
                break;
            }
        }
    }

    points = (whitePieces[0] + whitePieces[1] * 5 + whitePieces[2] * 3 + whitePieces[3] * 3 + whitePieces[4] * 9) - (blackPieces[0] + blackPieces[1] * 5 + blackPieces[2] * 3 + blackPieces[3] * 3 + blackPieces[4] * 9);

    return points;
}
