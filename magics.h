#ifndef MAGICS_H
#define MAGICS_H
#include <stdint.h>

typedef struct {
	uint64_t mask;
	uint64_t magic;
	uint8_t shift;
	int offset;
} MagicEntry;

#define ROOK_TABLE_SIZE 102400
#define BISHOP_TABLE_SIZE 5248

extern MagicEntry rookEntries[64];
extern MagicEntry bishopEntries[64];
extern uint64_t rookTable[ROOK_TABLE_SIZE];
extern uint64_t bishopTable[BISHOP_TABLE_SIZE];

void initMagics(void);

static inline int magicIndex(const MagicEntry *e, uint64_t occ) {
	return (int)(((occ & e->mask) * e->magic) >> e->shift) + e->offset;
}

static inline uint64_t getRookAttacks(int sq, uint64_t occ) {
	return rookTable[magicIndex(&rookEntries[sq], occ)];
}

static inline uint64_t getBishopAttacks(int sq, uint64_t occ) {
	return bishopTable[magicIndex(&bishopEntries[sq], occ)];
}

#endif
