#include "lmath.h"
#include "magics.h"
#include "play.h"
#include "uci.h"
#include <stdio.h>

int main() {
	init_between_bb();
	init_castling_mask();
	initMagics();
	init_pseudo_attacks();

	uciStart();
	printf("\n");
	return 0;
}
