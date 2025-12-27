#include <stdio.h>
#include "lmath.h"

int main(void){
    BitboardSet board = {0};
    char inputfen[200] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char offset = 0;
    for (unsigned char i = 0; i < sizeof(inputfen); i++)
    {
        switch (tolower(inputfen[i]))
        {
        case 'p':
            set_bit(board.pieces[0],i,1);
            break;
        case 'n':
            set_bit(board.pieces[2],i,1);
            break;
        case 'b':
            set_bit(board.pieces[1],i,1);
            break;
        case 'r':
            set_bit(board.pieces[3],i,1);
            break;
        case 'q':
            set_bit(board.pieces[4],i,1);
            break;
        case 'k':
            set_bit(board.pieces[5],i,1);
            break;
        case '/':

            break;
        default:
            break;
        }
        
    }
    
}