#ifndef LMATH_H
#define LMATH_H



unsigned long simple_rand(void);

unsigned char rand_between(unsigned char min, unsigned char max);

char normalize_sign(char num);

char absChar(int x);


char isUppercase(char c);

void append64char(char arr[], int value);

char abs1(int num);


char findMaxValue(char arr[], char size);
char findMinValue(int arr[], int size);

#endif