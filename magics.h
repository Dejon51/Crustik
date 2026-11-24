#ifndef MAGICS_H
#define MAGICS_H
#include <stdint.h>

#define ROOK_TABLE_SIZE   102400
#define BISHOP_TABLE_SIZE 5248

/* Enable PEXT only on x86_64 with hardware BMI2 unless forced off */
#if !defined(FORCE_MAGICS) && defined(__x86_64__) && defined(__BMI2__)
#define USE_PEXT 1
#include <immintrin.h>
#else
#define USE_PEXT 0
#endif

typedef struct {
	uint64_t mask;
	uint64_t magic;
	uint8_t shift;
	int offset;
} MagicEntry;

typedef struct {
	uint64_t mask;
	int offset;
} PextEntry;

#if USE_PEXT
extern PextEntry rookEntries[64];
extern PextEntry bishopEntries[64];
#else
extern MagicEntry rookEntries[64];
extern MagicEntry bishopEntries[64];
#endif

extern uint64_t rookTable[ROOK_TABLE_SIZE];
extern uint64_t bishopTable[BISHOP_TABLE_SIZE];

void initMagics(void);
void initPext(void);

static inline void initSliderAttacks(void) {
#if USE_PEXT
	initPext();
#else
	initMagics();
#endif
}

static inline int magicIndex(const MagicEntry *e, uint64_t occ) {
	return (int)(((occ & e->mask) * e->magic) >> e->shift) + e->offset;
}

#if USE_PEXT
static inline int pextIndex(const PextEntry *e, uint64_t occ) {
	return (int)_pext_u64(occ, e->mask) + e->offset;
}
#endif

static inline uint64_t getRookAttacks(int sq, uint64_t occ) {
#if USE_PEXT
	return rookTable[pextIndex(&rookEntries[sq], occ)];
#else
	return rookTable[magicIndex(&rookEntries[sq], occ)];
#endif
}

static inline uint64_t getBishopAttacks(int sq, uint64_t occ) {
#if USE_PEXT
	return bishopTable[pextIndex(&bishopEntries[sq], occ)];
#else
	return bishopTable[magicIndex(&bishopEntries[sq], occ)];
#endif
}

#endif