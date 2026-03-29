#ifndef LMATH_H
#define LMATH_H

#include <stdbool.h>
#include <stdint.h>


typedef uint64_t u64;
typedef u64 Bitboard;

typedef enum {
    WHITE_QUEENSIDE = 0,
    WHITE_KINGSIDE  = 1,
    BLACK_QUEENSIDE = 2,
    BLACK_KINGSIDE  = 3
} CastlingRight;


typedef struct {
    // color 0 is white
    // color 1 is black

    // pawn 0
    // bishop 1
    // horse 2
    // rook 3
    // queen 4
    // king 5
    Bitboard color[2];
    Bitboard pieces[6];
    int8_t epsquare;
    int8_t castling;
    uint8_t mailbox[64];
    uint8_t halfmoves;   // half-move clock
    uint8_t fullmoves; 
    uint64_t hash;
    bool turn;
} Position;

// struct Undo {
//     int capturedPiece;
//     int castlingRights;
//     int enPassantSquare;
//     int halfmoveClock;
// };

enum {
    PAWNNUMBER = 0,
    BISHOPNUMBER = 1,
    HORSENUMBER = 2,
    ROOKNUMBER = 3,
    QUEENNUMBER = 4,
    KINGNUMBER = 5,
};

#if __STDC_VERSION__ >= 202311L
    // C23: enums with fixed underlying type
    enum Side : uint8_t {
        WHITE = 0,
        BLACK = 1,
    };

    enum Square : uint8_t {
        A8, B8, C8, D8, E8, F8, G8, H8,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A1, B1, C1, D1, E1, F1, G1, H1,
        SQ_SQUARE
    };
#elif __STDC_VERSION__ >= 201710L
    // C17: plain enums
    enum Side {
        WHITE = 0,
        BLACK = 1,
    };

    enum Square {
        A8, B8, C8, D8, E8, F8, G8, H8,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A1, B1, C1, D1, E1, F1, G1, H1,
        SQ_SQUARE
    };
#else
    #error "C standard must be C17 or newer"
#endif



typedef struct {
    uint16_t movelist[256];
    unsigned offset;
} MoveList;

uint64_t get_time_ms();

int pop_lsb(Bitboard* bb);

bool isDigit(char c);

char mstrcmp(const char *s1, const char *s2);

uint64_t simple_rand(void);

unsigned int rand_between(unsigned int min, unsigned int max);

int normalize_sign(int num);

char isUppercase(char c);

void append64char(char arr[], int value);

char abs1(int num);

int findMaxValue(int arr[], int size);
int findMinValue(int arr[], int size);

int matoi(const char *str);


#endif