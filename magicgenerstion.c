#include <stdint.h>
#include <stdio.h>

typedef struct
{
    uint64_t data[262144];
    int offset[64];
} Attack;

Attack rookGenSquare(int pos, uint16_t square, Attack rookAttacks)
{
    int x = pos % 8;
    int y = pos / 8;

    // Direction vectors: NE, SE, SW, NW

    int dx[] = {0, 1, 0, -1};
    int dy[] = {1, 0, -1, 0};
    for (int lol = 0; lol < 64; lol++)
    {
        for (int currBB = 0; currBB < 4096; currBB++)
        {
            for (int l = 0; l < 12; l++)
            {
                for (int dir = 0; dir < 4; dir++)
                {
                    for (int i = 1; i < 7; i++)
                    {
                        int nx = x + dx[dir] * i;
                        int ny = y + dy[dir] * i;

                        if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                        {
                            break;
                        }
                        int target = nx + ny * 8;
                        if ((square >> l) & 1)
                        {
                            rookAttacks.data[rookAttacks.offset[lol]] |= (1ULL << target);
                        }
                    }
                }
            }
        }
}
square--;
return rookAttacks;
}

void rookMoves(Attack *attacks)
{
}

int main()
{

    return 1;
}