#ifndef LMATH_H
#define LMATH_H

#include <stdint.h>
#include <stdbool.h>


typedef uint64_t u64;
typedef u64 Bitboard;

typedef struct {
    Bitboard color[2];
    Bitboard pieces[6];
    // color 0 is white
    // color 1 is black

    // pawn
    // bishop
    // horse
    // rook
    // queen
    // king
} BitboardSet;


bool is_set(u64 value, int sq);

u64 set_bit(u64 value, int sq, int on);

bool isDigit(char c);

char mstrcmp(const char *s1, const char *s2);

int charToInt(char digit_char);

uint64_t simple_rand(void);

unsigned int rand_between(unsigned int min, unsigned int max);

int normalize_sign(int num);

char isUppercase(char c);

void append64char(char arr[], int value);

char abs1(int num);

int findMaxValue(int arr[], int size);
int findMinValue(int arr[], int size);

char tolower1(char character);
#endif