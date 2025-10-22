#ifndef READ_H
#define READ_H

#include <stdio.h>


void print_bytes(const char *label, const unsigned char *bytes, unsigned int num_bytes) {
    printf("%s [", label);          // Print a label and open bracket
    for (unsigned int i = 0; i < num_bytes; i++) {
        if (i > 0) printf(" ");     // Space between bytes
        for (int bit = 7; bit >= 0; bit--) {     // Print each bit from MSB to LSB
            printf("%c", (bytes[i] & (1 << bit)) ? '1' : '0');
        }
    }
    printf("]\n");                  // Close bracket and newline
}


#endif
