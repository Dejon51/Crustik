#include "magics.h"
#include <assert.h>
#include <immintrin.h>
#include <stddef.h>

PextEntry rookEntries[64];
PextEntry bishopEntries[64];
uint64_t rookTable[ROOK_TABLE_SIZE];
uint64_t bishopTable[BISHOP_TABLE_SIZE];

static const int rookDirs[4][2] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};
static const int bishopDirs[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};

static uint64_t rayAttacks(int sq, const int dirs[4][2], uint64_t occ) {
	uint64_t bb = 0;
	for (int dir = 0; dir < 4; dir++) {
		int file = sq & 7;
		int rank = sq >> 3;
		for (;;) {
			file += dirs[dir][0];
			rank += dirs[dir][1];

			if (file < 0 || file > 7 || rank < 0 || rank > 7)
				break;

			uint64_t bit = 1ULL << (rank * 8 + file);
			bb |= bit;
			if (occ & bit)
				break;
		}
	}
	return bb;
}

static uint64_t rookMask(int sq) {
	int file = sq & 7;
	int rank = sq >> 3;
	uint64_t mask = 0;
	for (int f = 1; f < 7; f++)
		mask |= 1ULL << (rank * 8 + f);
	for (int r = 1; r < 7; r++)
		mask |= 1ULL << (r * 8 + file);
	return mask & ~(1ULL << sq);
}

static uint64_t bishopMask(int sq) {
	const uint64_t edges = 0xFF818181818181FFULL;
	return rayAttacks(sq, bishopDirs, 0) & ~edges;
}

static void build(PextEntry *entries, uint64_t *table, size_t tableSize,
				  int isRook) {
	uint32_t offset = 0;
	for (int sq = 0; sq < 64; sq++) {
		PextEntry *e = &entries[sq];
		e->mask = isRook ? rookMask(sq) : bishopMask(sq);
		e->offset = offset;

		int bits = __builtin_popcountll(e->mask);
		uint64_t occ = 0;
		do {
			uint64_t attacks = isRook ? rayAttacks(sq, rookDirs, occ)
									  : rayAttacks(sq, bishopDirs, occ);
			int idx = pextIndex(e, occ);

			// Each subset produces a unique index; overwrite directly
			table[idx] = attacks;
			occ = (occ - e->mask) & e->mask;
		} while (occ);

		offset += 1U << bits;
	}
	assert((size_t)offset == tableSize);
}

void initPext(void) {
	build(rookEntries, rookTable, ROOK_TABLE_SIZE, 1);
	build(bishopEntries, bishopTable, BISHOP_TABLE_SIZE, 0);
}