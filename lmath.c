
#include "play.h"
#include "lmath.h"

unsigned long simple_rand(void)
{
    static unsigned long state = 0;

    if (state == 0)
    { // initialize state on first call
        int x, y;
        unsigned long seed = (unsigned long)&x;
        seed ^= ((unsigned long)&y << (sizeof(unsigned long) * 4)); // shift half of word size
        seed *= 2654435761UL;                                       // golden ratio multiplier
        seed ^= (seed >> 16);                                       // mix bits
        if (seed == 0)
            seed = 1; // avoid zero
        state = seed;
    }

    // LCG step (constants fit 32-bit and up; fast on 16-bit too)
    state = state * 1664525UL + 1013904223UL;

    return state;
}

unsigned char rand_between(unsigned char min, unsigned char max)
{
    if (max < min)
    { // swap if reversed
        unsigned long tmp = min;
        min = max;
        max = tmp;
    }
    unsigned long range = max - min + 1;
    return (simple_rand() % range) + min;
}

char normalize_sign(char num)
{
    if (num >= 0)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

char absChar(int x)
{
    return (x < 0) ? -x : x;
}

char isUppercase(char c)
{
    return c >= 'A' && c <= 'Z';
}

void append64char(char arr[], int value)
{
    int i;
    // printf("%i", value);

    for (i = 0; i < 64; i++)
    {
        if (arr[i] == 98)
        { // current code only fills empty with non-zero
            arr[i] = value;
            return;
        }
    }
}

char abs1(int num)
{
    if (num < 0)
    {
        return -num;
    }
    return num;
}

char findMaxValue(char arr[], char size)
{
    if (size <= 0)
    {
        // Handle empty or invalid array size
        return -1;
    }

    int max_value = arr[0];
    int max_index = 0;

    for (char i = 1; i < size; i++)
    {
        if (arr[i] > max_value)
        {
            max_value = arr[i];
            max_index = i;
        }
    }

    return max_value;
}
char findMinValue(int arr[], int size)
{
    if (size <= 0)
    {
        // Handle empty or invalid array size
        return -1;
    }

    int min_value = arr[0];
    int min_index = 0;

    for (int i = 1; i < size; i++)
    {
        if (arr[i] < min_value)
        {
            min_value = arr[i];
            min_index = i;
        }
    }
    return min_value;
}
