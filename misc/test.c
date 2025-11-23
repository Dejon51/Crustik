#include <stdio.h>

typedef unsigned long long Bitboard;

typedef struct
{
    Bitboard pawn;
    Bitboard horse;
    Bitboard bishop;
    Bitboard king;
    Bitboard queen;
    Bitboard rook;
    Bitboard black;
    Bitboard white;
} BitboardSet;

void set_bit(Bitboard *bb, int sq)
{
    *bb |= (1ULL << sq);
}

char is_set(Bitboard bb, int sq)
{
    return (bb & (1ULL << sq)) != 0;
}

// Convert 2D board to BitboardSet
void board_to_bitboards(char board[8][8], BitboardSet *bits)
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            int sq = row * 8 + col;
            char piece = board[row][col];
            switch (piece)
            {
            case 'P':
                set_bit(&bits->pawn, sq);
                bits->white |= (1ULL << sq);
                break;
            case 'p':
                set_bit(&bits->pawn, sq);
                bits->black |= (1ULL << sq);
                break;
            case 'N':
                set_bit(&bits->horse, sq);
                bits->white |= (1ULL << sq);
                break;
            case 'n':
                set_bit(&bits->horse, sq);
                bits->black |= (1ULL << sq);
                break;
            case 'B':
                set_bit(&bits->bishop, sq);
                bits->white |= (1ULL << sq);
                break;
            case 'b':
                set_bit(&bits->bishop, sq);
                bits->black |= (1ULL << sq);
                break;
            case 'R':
                set_bit(&bits->rook, sq);
                bits->white |= (1ULL << sq);
                break;
            case 'r':
                set_bit(&bits->rook, sq);
                bits->black |= (1ULL << sq);
                break;
            case 'Q':
                set_bit(&bits->queen, sq);
                bits->white |= (1ULL << sq);
                break;
            case 'q':
                set_bit(&bits->queen, sq);
                bits->black |= (1ULL << sq);
                break;
            case 'K':
                set_bit(&bits->king, sq);
                bits->white |= (1ULL << sq);
                break;
            case 'k':
                set_bit(&bits->king, sq);
                bits->black |= (1ULL << sq);
                break;
            }
        }
    }
}

// Convert BitboardSet back to 2D board
void bitboards_to_board(BitboardSet *bits, char board[8][8])
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            int sq = row * 8 + col;
            if (is_set(bits->pawn, sq))
                board[row][col] = is_set(bits->white, sq) ? 'P' : 'p';
            else if (is_set(bits->horse, sq))
                board[row][col] = is_set(bits->white, sq) ? 'N' : 'n';
            else if (is_set(bits->bishop, sq))
                board[row][col] = is_set(bits->white, sq) ? 'B' : 'b';
            else if (is_set(bits->rook, sq))
                board[row][col] = is_set(bits->white, sq) ? 'R' : 'r';
            else if (is_set(bits->queen, sq))
                board[row][col] = is_set(bits->white, sq) ? 'Q' : 'q';
            else if (is_set(bits->king, sq))
                board[row][col] = is_set(bits->white, sq) ? 'K' : 'k';
            else
                board[row][col] = '.';
        }
    }
}

int main()
{
    char board[8][8] = {
        {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
        {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
        {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}};

    BitboardSet bits = {0};
    board_to_bitboards(board, &bits);

    // Print some checks
    printf("Bitboards of pawns:\n");
    for (int i = 0; i < 64; i++)
    {
        printf("%d", is_set(bits.pawn, i));
        if (i % 8 == 7)
            printf("\n");
    }

    // Convert back to board
    char new_board[8][8];
    bitboards_to_board(&bits, new_board);

    printf("\nBoard reconstructed from bitboards:\n");
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            printf("%c ", new_board[r][c]);
        }
        printf("\n");
    }

    return 0;
}
