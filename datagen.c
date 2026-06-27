// datagen.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "play.h"
#include "fen.h"
#include "search.h"
#include "tt.h"
#include "lmath.h"

#define DATAGEN_FENS 10000
#define FILTER_DEPTH 10

typedef struct {
    char **fens;
    size_t count;
} FenBook;

static FenBook load_book(const char *filename) {
    FenBook book = {0};
    FILE *f = fopen(filename, "r");
    if (!f) return book;

    char buf[512];

    while (fgets(buf, sizeof(buf), f))
        book.count++;

    if (!book.count) {
        fclose(f);
        return book;
    }

    book.fens = malloc(book.count * sizeof(char *));
    if (!book.fens) {
        fclose(f);
        book.count = 0;
        return book;
    }

    rewind(f);

    size_t i = 0;
    while (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        book.fens[i++] = strdup(buf);
    }

    fclose(f);
    return book;
}

static void free_book(FenBook *book) {
    for (size_t i = 0; i < book->count; i++)
        free(book->fens[i]);

    free(book->fens);
    book->fens = NULL;
    book->count = 0;
}

static int read_fen_string(Position *board, const char *fen) {
    char temp[512];
    strncpy(temp, fen, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *tok[6];
    int t = 0;

    char *p = strtok(temp, " ");
    while (p && t < 6) {
        tok[t++] = p;
        p = strtok(NULL, " ");
    }

    if (t < 6)
        return 0;

    fenRead(board, tok[0], tok[1], tok[2], tok[3], tok[4], tok[5]);

    board->color[2] = board->color[0] | board->color[1];

    return 1;
}

static char piece_from_bitboards(Position *b, int sq) {
    uint64_t bb = 1ULL << sq;

    int side;
    if (b->color[0] & bb)
        side = 0;
    else if (b->color[1] & bb)
        side = 1;
    else
        return 0;

    if (b->pieces[0] & bb) return side == 0 ? 'P' : 'p';
    if (b->pieces[1] & bb) return side == 0 ? 'B' : 'b';
    if (b->pieces[2] & bb) return side == 0 ? 'N' : 'n';
    if (b->pieces[3] & bb) return side == 0 ? 'R' : 'r';
    if (b->pieces[4] & bb) return side == 0 ? 'Q' : 'q';
    if (b->pieces[5] & bb) return side == 0 ? 'K' : 'k';

    return 0;
}

static void generate_fen(Position *b, char *out, size_t out_size) {
    size_t pos = 0;

    b->color[2] = b->color[0] | b->color[1];

    for (int rank = 0; rank < 8; rank++) {
        int empty = 0;

        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            char c = piece_from_bitboards(b, sq);

            if (!c) {
                empty++;
            } else {
                if (empty) {
                    pos += snprintf(out + pos, out_size - pos, "%d", empty);
                    empty = 0;
                }

                pos += snprintf(out + pos, out_size - pos, "%c", c);
            }
        }

        if (empty)
            pos += snprintf(out + pos, out_size - pos, "%d", empty);

        if (rank != 7)
            pos += snprintf(out + pos, out_size - pos, "/");
    }

    pos += snprintf(out + pos, out_size - pos, " %c ", b->turn ? 'b' : 'w');

    int castle = 0;

    if (b->castling & (1U << WHITE_KINGSIDE)) {
        pos += snprintf(out + pos, out_size - pos, "K");
        castle = 1;
    }
    if (b->castling & (1U << WHITE_QUEENSIDE)) {
        pos += snprintf(out + pos, out_size - pos, "Q");
        castle = 1;
    }
    if (b->castling & (1U << BLACK_KINGSIDE)) {
        pos += snprintf(out + pos, out_size - pos, "k");
        castle = 1;
    }
    if (b->castling & (1U << BLACK_QUEENSIDE)) {
        pos += snprintf(out + pos, out_size - pos, "q");
        castle = 1;
    }

    if (!castle)
        pos += snprintf(out + pos, out_size - pos, "-");

    pos += snprintf(out + pos, out_size - pos, " ");

    if (b->epsquare == -1) {
        pos += snprintf(out + pos, out_size - pos, "-");
    } else {
        int sq = b->epsquare;
        pos += snprintf(out + pos, out_size - pos,
                        "%c%d",
                        'a' + (sq & 7),
                        8 - (sq >> 3));
    }

    snprintf(out + pos, out_size - pos,
             " %u %u",
             (unsigned)b->halfmoves,
             (unsigned)b->fullmoves);
}

static uint8_t play_rand_moves(Position *pos, uint8_t rand_moves) {
    if (rand_moves == 0) {
        MoveList moves = {0};
        legalMoveGen(pos, &moves);

        if (moves.offset == 0)
            return 0;

        stopConditions stop = {0};
        PVLine pv = {0};

        tt_clear();
        clear_ordering_tables();

        searchOutput out = search(pos, FILTER_DEPTH, 0, -32000, 32000, &stop, &pv);

        if (abs(out.score) > 1000)
            return 0;

        char fen[256];
        generate_fen(pos, fen, sizeof(fen));

        printf("info string genfens %s\n", fen);
        return 1;
    }

    MoveList moves = {0};
    legalMoveGen(pos, &moves);

    if (moves.offset == 0)
        return 0;

    int move_index = rand() % moves.offset;

    Position copy = *pos;
    makeMove(&copy, &moves, move_index);

    return play_rand_moves(&copy, rand_moves - 1);
}

void genfens(uint64_t seed, uint16_t n_of_fens, const char *bookfile) {
    srand((unsigned int)seed);

    FenBook book = {0};
    int use_book = bookfile && strcmp(bookfile, "None") != 0;

    if (use_book) {
        book = load_book(bookfile);
        if (!book.count)
            return;
    }

    int generated = 0;

    while (generated < n_of_fens) {
        Position pos = {0};

        if (use_book) {
            const char *fen = book.fens[rand() % book.count];

            if (!read_fen_string(&pos, fen))
                continue;
        } else {
            fenRead(&pos,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                    "w", "KQkq", "-", "0", "1");

            pos.color[2] = pos.color[0] | pos.color[1];
        }

        Position copy = pos;
        int random_moves = 6 + rand() % 4;

        if (play_rand_moves(&copy, random_moves))
            generated++;
    }

    if (use_book)
        free_book(&book);
}

int datagen(void) {
    genfens((uint64_t)time(NULL), DATAGEN_FENS, "None");
    return 0;
}