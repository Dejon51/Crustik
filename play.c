#include "play.h"
#include "lmath.h"
#include "moves.h"

// Fuction is air tight -----------------------------------------------------------
PieceLocation locatePiece(char piece, char id, char board[8][8]) {
    PieceLocation piecelocation;
    // printf("%i\n",id);
    char count = 0; 

    for (char x = 0; x < 8; x++) {
        for (char y = 0; y < 8; y++) {
            if (
                board[y]
                [x] == piece) {  // Check if the piece matches
                count++;
                if (count == id) {  // If it's the Nth occurrence

                    piecelocation.x = x;
                    piecelocation.y = y;
                    return piecelocation;  // Return immediately
                }
            }
        }
    }

    // If piece not found, return invalid location
    piecelocation.x = -1;
    piecelocation.y = -1;
    return piecelocation;
}
void makeMove(char board[8][8]){
    char piece = 'p';
    char candid[64] = {0};

    
    PieceArray result1 = avalibleMoves(piece,board);
    char *result = result1.data;   // keep it as pointer/array
    char replacementArr[64] = {};
    for (char i = 0; i < 64; i++)   // use actual length, not sizeof
    {    
        if (result[i] == 98) result[i] = 0;
        replacementArr[i] = result[i]; // Keep EYE ON HERE )))))))))))))))))))))))))))))))))))
    }
    printf("\n");

    char tmp = findMaxValue(replacementArr,64);
    int occurrence = 0;
    char sepcount = 0; // End this pain
    char tmpocu = 0;
    for (char i = 0; i < 64; i++) {
        if (replacementArr[i] != 0) {
            if (result[i] == tmp) { // Determines if a pawn gets chosen for a move
                sepcount++;
                tmpocu = sepcount;
            }
        }
    }
    occurrence = 0;
    sepcount = 0;
    char choosenNUM = rand_between(1,tmpocu);
    printf("\n\n%i\n\n",choosenNUM);
    for(int i = 0; i < 64; i++){
            occurrence++;
            // printf("%i = %i, count: %i \n\n",result[i],tmp,occurrence);
            // printf("%iocu\n",occurrence);
        if(replacementArr[i] != 0){


            if(result[i] == tmp){ // Determines if a pawn gets choosen for a move
                sepcount++;
                printf("j%i\n\n",result[i]);
                printf("%i\n\n",occurrence);


                    if(sepcount == choosenNUM){


                        PieceLocation loc = locatePiece(piece, choosenNUM, board);
                        PawnMoves a = pawn(-0,1,loc.x,loc.y,board);
                        printf("%i ",a.data[0]);
                        printf("%i ",a.data[2]);

                        if(a.data[0] > a.data[2]){
                            
                            board[loc.y+1][loc.x] = piece;

                            board[loc.y][loc.x] = '.';
                        }else
                        {
                            board[loc.y+1][loc.x] = piece;

                            board[loc.y][loc.x] = '.';
                        }
                        printf("found correct value %i x: %i y: %i\n", tmp, loc.x, loc.y);
                }
            }
        }
    }
    
}





