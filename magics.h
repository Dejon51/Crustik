#ifndef PEXT_H
#define PEXT_H
#include <stdint.h>

#if defined(__BMI2__)
#include <immintrin.h>
#endif

typedef struct {
	uint64_t mask;
	int offset;
} PextEntry;

#define ROOK_TABLE_SIZE 102400
#define BISHOP_TABLE_SIZE 5248

extern PextEntry rookEntries[64];
extern PextEntry bishopEntries[64];
extern uint64_t rookTable[ROOK_TABLE_SIZE];
extern uint64_t bishopTable[BISHOP_TABLE_SIZE];

void initPext(void);

static inline int pextIndex(const PextEntry *e, uint64_t occ) {
#if defined(__BMI2__)
	return (int)_pext_u64(occ, e->mask) + e->offset;
#else
	uint64_t m = e->mask;
	uint64_t res = 0;
	uint64_t bit = 1ULL;
	while (m) {
		uint64_t lsb = m & -m;
		if (occ & lsb)
			res |= bit;
		bit <<= 1;
		m ^= lsb;
	}
	return (int)res + e->offset;
#endif
}

static inline uint64_t getRookAttacks(int sq, uint64_t occ) {
	return rookTable[pextIndex(&rookEntries[sq], occ)];
}

static inline uint64_t getBishopAttacks(int sq, uint64_t occ) {
	return bishopTable[pextIndex(&bishopEntries[sq], occ)];
}

#endif