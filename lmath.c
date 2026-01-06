
#include "play.h"
#include "lmath.h"

bool is_set(u64 value, int sq)
{
    if ((unsigned)sq >= 64) // ensure index is within bounds
        return 0;

    return (value >> sq) & 1;
}

u64 set_bit(u64 value, int sq, int on)
{
    if ((unsigned)sq >= 64)
        return value;

    u64 mask = 1ULL << sq;
    return (value & ~mask) | (-((u64)on) & mask);
}

uint16_t pack6(uint8_t a, uint8_t b)
{
    // a and b must be 0–63
    return (a & 63) | ((b & 63) << 6);
}

uint8_t unpack6(uint16_t v, uint8_t which)
{
    return (which == 0)
        ? (v & 63)          // bits 0–5
        : ((v >> 6) & 63);  // bits 6–11
}

bool isDigit(char c) {
    if (c >= '0' && c <= '9') {
        return 1; // It is a digit
    } else {
        return 0; // It is not a digit
    }
}

char mstrcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) { 
        if (*s1 != *s2) {
            return *s1 - *s2;    
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;        
}

int charToInt(char digit_char) {
    if (digit_char >= '0' && digit_char <= '9') {
        return digit_char - '0';
    } else {
        // Handle non-digit characters, e.g., return an error code
        return -1; // Or throw an error, depending on desired error handling
    }
}


uint64_t simple_rand(void)
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

unsigned int rand_between(unsigned int min, unsigned int max)
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

int normalize_sign(int num)
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

int findMaxValue(int arr[], int size)
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
int findMinValue(int arr[], int size)
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

void itoa(int value, char *buf) {
    char tmp[20];
    int i = 0, j, neg = 0;

    if (value < 0) {
        neg = 1;
        value = -value;
    }

    do {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    if (neg) tmp[i++] = '-';

    // reverse string
    for (j = 0; j < i; j++) {
        buf[j] = tmp[i - j - 1];
    }
    buf[i] = '\0';
}

