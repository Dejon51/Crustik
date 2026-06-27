#ifndef DATAGEN_H
#define DATAGEN_H

#include <stdint.h>

void genfens(uint64_t seed, uint16_t n_of_fens, const char *bookfile);
int datagen(void);

#endif