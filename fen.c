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

Position fenRead(char *fen,char *arg1,char *arg2,char *arg3,char *arg4,char *arg5)
{
    
    bool invalidfen = 0;
    Position board = {0};
    int square = 0;
    // char fen[200] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    for (int i = 0; fen[i] != '\0'; i++)
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
            if (fen[i] >= '1' && fen[i] <= '8'|| fen[i] == '\0'){
                square += fen[i] - '0';
            }
            else{
                printf("hewokadas: %c",fen[i]);
                memset(&board, 0, sizeof(Position));
                return board;
                break;
            }
            break;
        }
    }
    
    if (arg1[0] == 'w')
    {
        board.turn = 0;
    }
    else if(arg1[0] == 'b'){
        board.turn = 1;
    }
    else
    {
        printf("hewokadas: %c",arg1[0]);
        memset(&board, 0, sizeof(Position));
        return board;
    }
    
    
    
    print_binary(board.pieces[0]);
    print_binary(board.pieces[1]);
    print_binary(board.pieces[2]);
    print_binary(board.pieces[3]);
    print_binary(board.pieces[4]);
    print_binary(board.pieces[5]);
    printf("%i",board.turn);

    return board;
}