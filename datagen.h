#ifndef DATAGEN_H
#define DATAGEN_H

// Generate random FENs from random games
// Usage: genfens <count> seed <seed> book <book> [plies=<plies>]
//   count - number of FENs to generate
//   seed - random seed
//   book - opening book file or "none" for startpos
//   plies - exact number or range (e.g., plies=8 or plies=6-10)
void datagen_genfens(int argc, char **argv);

#endif // DATAGEN_H