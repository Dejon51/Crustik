#include <stdio.h>
#include "eval.h"
#include "moves.h"
#include "play.h"

char board1[8][8] = {
    {'r','n','b','q','k','b','n','r'},  // Black back rank
    {'p','p','p','p','p','p','p','p'},  // Black pawns
    {'.','.','.','.','.','.','.','.'},  // Empty row
    {'.','.','.','.','.','.','.','.'},  // Empty row
    {'.','.','.','.','.','.','.','.'},  // Empty row
    {'.','.','.','.','.','.','.','.'},  // Empty row
    {'P','P','P','P','P','P','P','P'},  // White pawns
    {'R','N','B','Q','K','B','N','R'}   // White back rank
};
char board[8][8] = {
    {'r','n','.','q','.','Q','n','r'},  // Black back rank
    {'pcoul','p','p','p','p','p','p','p'},  // Black pawns
    {'.','.','.','.','R','.','R','.'},  // Empty row
    {'.','.','.','.','.','.','.','.'},  // Empty row
    {'.','.','.','r','r','.','.','.'},  // Empty row
    {'.','.','P','.','.','P','.','.'},  // Empty row
    {'.','.','.','.','.','.','.','.'},  // White pawns
    {'.','.','.','.','.','.','.','.'}   // White back rank
};

int main(){
    PieceLocation p = locatePiece('p',2,board);

    printf("%in %in\n",p.x,p.y);

    makeMove(board);

    for (char x = 0; x < 8; x++)
    {
        for (char y = 0; y < 8; y++)
    {
        printf("%c ",board[x][y]);
    }
    printf("\n");
    }
    
    printf("\n");
    
    
    return 1;
}