
#include "play.h"
#include "lmath.h"
#include <time.h>


uint64_t get_time_ms()
{
    struct timespec ts;
#ifdef __linux__
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

    return (ts.tv_sec * 1000L) + (ts.tv_nsec / 1000000L);
}

int pop_lsb(Bitboard* bb) {
  const int sq = __builtin_ctzll(*bb);
  *bb &= *bb - 1;
  return sq;
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

int matoi(const char *str) 
{
    int result = 0;
    int sign = 1;
    int i = 0;

    if (str[i] == '-')
    {
        sign = -1;
        i++;
    }
    else if (str[i] == '+')
    {
        i++;
    }

    while (str[i] >= '0' && str[i] <= '9')
    {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result * sign;
}

// #define LN2 0.69314718056f
// #define SQRT2 1.41421356237f

// float my_logf(float x) {
//     if (x <= 0.0f) return -1.0f / 0.0f;
    
//     union { float f; unsigned int i; } conv;
//     conv.f = x;
//     unsigned int ix = conv.i;

//     int exp = (int)(ix >> 23) - 127;
    
//     conv.i = (ix & 0x7FFFFF) | 0x3F800000;
//     float m = conv.f;
//     if (m > SQRT2) {
//         m *= 0.5f;
//         exp++;
//     }

//     float f = m - 1.0f;
//     float f2 = f * f;
//     float f3 = f2 * f;
//     float poly = f  * (1.0000000f + f * (-0.4999999f + f * (0.3333330f + 
//                  f * (-0.2500068f + f * (0.1999468f + f * (-0.1652093f))))));

//     return poly + (float)exp * LN2;
// }