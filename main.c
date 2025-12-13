#include <stdio.h>
#include "eval.h"
#include "play.h"
#include "lmath.h"


Bitboard white = 0b1101001101111111100000000000000000011000000000000000000000000000;
Bitboard black =  0b0000010000000000000010100000000000000000001001000010000000000000;
Bitboard pawnS = 0b0000000011111111000000000000000000000000001001000000000000000000;
Bitboard bishop = 0b0000000000000000000000000000000000000000000000000010000000000000;
Bitboard horse = 0;
Bitboard rook = 0;
Bitboard queen = 0;
Bitboard king = 0;

// char board1[8][8] = {
//     {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}, // Black back ROWS
//     {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, // Black pawns
//     {'.', '.', '.', '.', '.', '.', '.', '.'}, // Empty row
//     {'.', '.', '.', '.', '.', '.', '.', '.'}, // Empty row
//     {'.', '.', '.', '.', '.', '.', '.', '.'}, // Empty row
//     {'.', '.', '.', '.', '.', '.', '.', '.'}, // Empty row
//     {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}, // White pawns
//     {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}  // White back ROWS
// };

// char board[8][8] = {
//     {'r', 'n', '.', 'q', '.', 'Q', 'n', 'r'}, // Black back ROWS
//     {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, // Black pawns
//     {'.', '.', '.', '.', 'R', '.', 'R', '.'}, // Empty row
//     {'.', '.', '.', '.', '.', '.', '.', '.'}, // Empty row
//     {'.', '.', '.', 'r', 'r', '.', '.', '.'}, // Empty row
//     {'.', '.', 'P', '.', '.', 'P', '.', '.'}, // Empty row
//     {'.', '.', '.', '.', '.', '.', '.', '.'}, // White pawns
//     {'.', '.', '.', '.', '.', '.', '.', '.'}  // White back ROWS
// };

int main()
{

    BitboardSet board = {0};
    board.color[0] = white;
    board.color[1] = black;
    board.pieces[0] = pawnS;
    board.pieces[1] = bishop;
    board.pieces[2] = horse;
    board.pieces[3] = rook;
    board.pieces[4] = queen;
    board.pieces[5] = king;

    makeMove(board,0);// 0 white black 1
    printf("\n");

    return 1;
}
