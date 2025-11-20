#include "play.h"
#include "lmath.h"
#include "moves.h"

// Function is air tight -----------------------------------------------------------
PieceLocation locatePiece(char piece, char id, char board[8][8])
{
    PieceLocation piecelocation;
    char count = 0;

    for (char y = 0; y < 8; y++)
    {
        for (char x = 0; x < 8; x++)
        {
            if (board[y][x] == piece)
            { // Check if the piece matches
                count++;
                if (count == id)
                { // If it's the Nth occurrence
                    piecelocation.x = x;
                    piecelocation.y = y;
                    return piecelocation; // Return immediately
                }
            }
        }
    }

    // If piece not found, return invalid
    piecelocation.x = -1;
    piecelocation.y = -1;
    return piecelocation;
}

void makeMove(char board[8][8])
{

    char piece = 'p';

    PieceArray result1 = avalibleMoves(piece, board);
    char *result = result1.data;
    char replacementArr[64] = {0};

    // copy and clean the data
    for (char i = 0; i < 64; i++)
    {
        if (result[i] == 98)
            result[i] = 0;
        replacementArr[i] = result[i];
    }
    // printf("\n");

    // Find the maximum value
    char tmp = findMaxValue(replacementArr, 64);
    // printf("Max value found: %i\n", tmp);

    // count how many moves have the max value and store their piece ids
    char maxMovePieceIDs[64] = {0};
    char maxMoveCount = 0;

    for (char i = 0; i < 64; i++)
    {
        if (replacementArr[i] != 0 && result[i] == tmp)
        {
            maxMovePieceIDs[maxMoveCount] = i + 1; // piece id is index + 1
            maxMoveCount++;
            // printf("Found max value at index %i (value: %i), piece ID: %i\n",
            //        i, result[i], i + 1);
        }
    }

    if (maxMoveCount == 0)
    {
        // printf("Error: No valid moves found\n");
        return;
    }

    // choose a random piece using the max value
    char chosenIndex = rand_between(0, maxMoveCount - 1);
    char pieceID = maxMovePieceIDs[chosenIndex];

    // printf("chosen piece: %i out of %i options\n", pieceID, maxMoveCount);

    PieceLocation loc = locatePiece(piece, pieceID, board);
    if (loc.x == -1 || loc.y == -1)
    {
        // printf("Error: Could not locate piece %i\n", pieceID);
        return;
    }

    // printf("Piece location: x=%i, y=%i\n", loc.x, loc.y);

    // get detailed moves for this specific pawn
    // in pawn function: color 0 gets direction +1, color 1 gets direction -1
    // so black pawns lowercase 'p' use color 0 and move down row increases
    // white pawns uppercase 'P' use color 1 and move up row decreases
    char pawnColor = (piece == 'p') ? 0 : 1;
    int direction = (pawnColor == 0) ? 1 : -1;

    PawnMoves a = pawn(pawnColor, 1, loc.y, loc.x, board);
    // printf("Move values: left=%i, straight=%i, right=%i\n", a.data[0], a.data[1], a.data[2]);

    // Get absolute values for comparison capture can be negative for white pawns
    int leftVal = abs1(a.data[0]);
    int rightVal = abs1(a.data[2]);

    // execute the move based on which has the highest value
    // assesssquare returns values where enemy pieces are positive capturable
    // 10 for friendly pieces or 0 for empty
    if (leftVal > 0 && leftVal != 10 && leftVal >= rightVal)
    {
        // Move pawn diagonally left (capture)
        if (loc.y + direction >= 0 && loc.y + direction < 8 && loc.x > 0)
        {
            board[loc.y + direction][loc.x - 1] = piece;
            board[loc.y][loc.x] = '.';
            // printf("moved diagonally left from (%i,%i) to (%i,%i)\n",
            //    loc.x, loc.y, loc.x - 1, loc.y + direction);
        }
    }
    else if (rightVal > 0 && rightVal != 10)
    {
        // move pawn diagonally right (capture)
        if (loc.y + direction >= 0 && loc.y + direction < 8 && loc.x < 7)
        {
            board[loc.y + direction][loc.x + 1] = piece;
            board[loc.y][loc.x] = '.';
            // printf("moved diagonally right from (%i,%i) to (%i,%i)\n",
            //    loc.x, loc.y, loc.x + 1, loc.y + direction);
        }
    }
    else if (a.data[1] != 0)
    {
        // move forward
        if (loc.y + direction >= 0 && loc.y + direction < 8)
        {
            board[loc.y + direction][loc.x] = piece;
            board[loc.y][loc.x] = '.';
            // printf("moved forward from (%i,%i) to (%i,%i)\n",
            //    loc.x, loc.y, loc.x, loc.y + direction);
        }
    }
    else
    {
        // printf("No move available for this piece\n");
    }

    // printf("move made: value=%i, from (%i,%i)\n", tmp, loc.x, loc.y);
}
