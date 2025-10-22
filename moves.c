#include "moves.h"


#include <stdio.h>
#include "lmath.h"

// Out bounds


int assessSquare(char color1,char x, char y, char board[8][8]) {
    // Prevent out-of-bounds access
    char color;
    // printf("%i",color1);

    if (color1 == 0)
    {
        color = 1;
    }
    if (color1 == 1)
    {
        color = 0;
    }
    
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return 11;
    }
    if (board[x][y]=='K')
    {
        if (color == 1)
        {
        return 12;
        }
        else{
            return 11;
        }
    }
    if (board[x][y]=='k')
    {
        if (color == 0)
        {
        return 12;
        }
        else{
            return 11;
        }
    }
    
    if (board[x][y] == 'Q') {
        if (color == 1)
        {
            return 9;
        }
        else{
            return 10;
        }
        

    } else if (board[x][y] == 'R') {
        if (color == 1)
        {
        return 5;
        }
        else{
            return 10;
        }
    } else if (board[x][y] == 'N' || 
               board[x][y] == 'B') {
        if (color == 1)
        {
        return 3;
        }
        else{
            return 10;
        }
    } else if (board[x][y] == 'P') {
        if (color == 1)
        {
        return 1;
        }
        else{
            return 10;
        }
    }
        if (board[x][y] == 'q') {
        if (color == 0)
        {
            return 9;
        }
        else{
            return 10;
        }
        

    } else if (board[x][y] == 'r') {
        if (color == 0)
        {
        return 5;
        }
        else{
            return 10;
        }
    } else if (board[x][y] == 'n' || 
               board[x][y] == 'b') {
        if (color == 0)
        {
        return 3;
        }
        else{
            return 10;
        }
    } else if (board[x][y] == 'p') {
        if (color == 0)
        {
        return 1;
        }
        else{
            return 10;
        }
    } else {
        return 0;
    }
}

PawnMoves pawn(char color,char detail, char x, char y, char board[8][8]) {
    // Summary of how the pawn is doing for program to decide what to do with said pawn
    char pawnStatus = 0;
    // assuming color 0 = white moving "up" (-1), 1 = black moving "down" (+1)

    int direction = 1;
    if (color == 1)
    {
        direction = direction*-1;
    }
    if (color == 0)
    {
        direction = direction*1;
    }
    PawnMoves moves = {0,1*direction,0,0};

    // forward
    if (x + direction >= 0 && x + direction < 8) {
        if (assessSquare(color, x + direction, y, board))
        {
            if (detail)
            {
                moves.data[1] = 0;
            }
            

            
        }
        else{
            if (assessSquare(color, x + direction, y, board) == 0)
            {
                if (detail){
                moves.data[3] +=1;
                }
            }
        }
    }

    // capture left
if (x + direction >= 0 && x + direction < 8 && y - 1 >= 0) {
    int left = assessSquare(color, x + direction, y - 1, board);
    if (left != 0) {           // if there is an enemy piece
        if (detail == 1){
            moves.data[3] += 1;
            moves.data[0] = left * direction;
            
        }
        else{
            if(left != 10){

            pawnStatus += left;
            }
        }
    }

}

// capture right
if (x + direction >= 0 && x + direction < 8 && y + 1 < 8) {
    int right = assessSquare(color, x + direction, y + 1, board);
    if (right != 0) {
        if (detail == 1){
        moves.data[3] += 1;
        moves.data[2] = right * direction;
        }
        else{
            if(right != 10){
            pawnStatus += right;
            }
        }
    }

}

    // printf("\n");
    
    if (detail){
    return moves;
    }
    if(!detail){
        moves.data[4] = abs1(pawnStatus);
        return moves;
    }
}


PieceArray avalibleMoves(char piece,char board[8][8]){
    PieceArray pawnsarray = {0};
    for (int i = 0; i < 64; i++) {
    pawnsarray.data[i] = 98;
    }
    for (char x = 0; x < 8; x++) {
        for (char y = 0; y < 8; y++) {
            switch (board[y][x]) {
                case 'P': 
                if (piece == 'P') // Do not remove Important for some reason
                {
                    PawnMoves moves = pawn(1,0,y,x,board);
                    printf("%ia ",moves.data[4]);
                    append64char(pawnsarray.data,moves.data[4]+penaltymap[y+8*x]);
                    // printf("%i",moves.data[4]);
                }
                break;
                case 'R': 
                
                break;
                case 'N': 
                
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
                    PawnMoves moves = pawn(0,0,y,x,board);
                    printf("%ia67 ",moves.data[4]);
                    append64char(pawnsarray.data,moves.data[4]+penaltymap[y+8*x]);
                }
                break;
                case 'r': 
                
                break;
                case 'n': 
                
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
