#include "lmath.h"
#include <stdint.h>
#include <stdio.h>

uint64_t bishopMoves(int ind) {
  uint64_t output = 0ULL;

  int x = ind % 8;
  int y = ind / 8;

  // Direction vectors: NE, SE, SW, NW
  int dx[] = {1, 1, -1, -1};
  int dy[] = {1, -1, -1, 1};
  int offset[] = {0, 7, 14, 21};

  for (int dir = 0; dir < 4; dir++) {
    for (int i = 1; i < 8; i++) {
      int nx = x + dx[dir] * i;
      int ny = y + dy[dir] * i;

      // Check bounds first
      if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) {
        break;
      }
      int target = nx + ny * 8;
      output |= (1ULL << target);
    }
  }
  return output;
}

uint64_t rookMoves(int ind) {
  uint64_t output = 0ULL;
  int x = ind % 8;
  int y = ind / 8;

  // Direction vectors: NE, SE, SW, NW
  int dx[] = {0, 1, 0, -1};
  int dy[] = {1, 0, -1, 0};
  int offset[] = {0, 7, 14, 21};

  for (int dir = 0; dir < 4; dir++) {
    for (int i = 1; i < 8; i++) {
      int nx = x + dx[dir] * i;
      int ny = y + dy[dir] * i;

      if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) {
        break;
      }
      int target = nx + ny * 8;
      output |= (1ULL << target);
    }
  }
  return output;
}

int main() {
  FILE *fptr;
  fptr = fopen("magics.h", "w");
  fprintf(fptr, "uint64_t rookmagic[] = {\n");

  for (int i = 0; i < 64; i++) {
    uint64_t moves = rookMoves(i);
    fprintf(fptr, "0x%llxULL,\n", moves);
  }
  fprintf(fptr, "};\n");

  fprintf(fptr, "uint64_t bishopmagic[] = {\n");
  for (int i = 0; i < 64; i++) {
    uint64_t moves = bishopMoves(i);
    fprintf(fptr, "0x%llxULL,\n", moves);
  }
  fprintf(fptr, "};\n");

  fclose(fptr);
}