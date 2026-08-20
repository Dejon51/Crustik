#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "datagen.h"
#include "play.h"          
#include "search.h"        
#include "eval.h"          
#include "tt.h"
#include "zobrist.h"
#include "fen.h"          

#define MATE_SCORE 32000
#define MAX_DEPTH 200

#define OPENING_GEN_TIMEOUT_MS 10000   // bail out of opening generation if we can't find a valid line
#define SEARCH_MAX_TIME_MS     3000    // hard wall-clock cap per move, independent of node counting

static uint64_t rng_state;

static void rng_seed(uint64_t seed) {
    rng_state = seed ? seed : 1;
}

static uint64_t rng_rand64(void) {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

static int rng_uniform(int n) {
    return (int)(rng_rand64() % (uint64_t)n);
}

static bool is_square_attacked(Position *board, int sq, int by_color) {
    return squareAttacked(board, sq, by_color);
}

static bool is_position_valid(Position *board) {
    int white_kings = 0, black_kings = 0;
    for (int sq = 0; sq < 64; sq++) {
        int piece = piece_on_square(board, sq);
        if (piece == 5) { // King
            if ((board->color[0] >> sq) & 1) white_kings++;
            else if ((board->color[1] >> sq) & 1) black_kings++;
        }
    }
    if (white_kings != 1 || black_kings != 1) return false;

    int prev_turn = board->turn ^ 1;
    uint64_t enemy_king_bb = board->pieces[5] & board->color[prev_turn];
    if (enemy_king_bb) {
        int king_sq = __builtin_ctzll(enemy_king_bb);
        if (is_square_attacked(board, king_sq, board->turn)) {
            return false;
        }
    }

    return true;
}

static void fen_from_position(Position *board, char *fen) {
    int idx = 0;
    for (int rank = 0; rank < 8; rank++) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            int piece = piece_on_square((Position *)board, sq);
            if (piece == -1) {
                empty++;
            } else {
                if (empty) {
                    fen[idx++] = '0' + empty;
                    empty = 0;
                }
                int white = (board->color[0] >> sq) & 1;
                const char pchar[] = "PBNRQK";
                fen[idx++] = white ? pchar[piece] : tolower(pchar[piece]);
            }
        }
        if (empty) fen[idx++] = '0' + empty;
        if (rank < 7) fen[idx++] = '/';
    }
    fen[idx++] = ' ';
    fen[idx++] = (board->turn == 0) ? 'w' : 'b';
    fen[idx++] = ' ';

    int cr = board->castling;
    if (cr == 0) {
        fen[idx++] = '-';
    } else {
        if (cr & (1 << WHITE_KINGSIDE))  fen[idx++] = 'K';
        if (cr & (1 << WHITE_QUEENSIDE)) fen[idx++] = 'Q';
        if (cr & (1 << BLACK_KINGSIDE))  fen[idx++] = 'k';
        if (cr & (1 << BLACK_QUEENSIDE)) fen[idx++] = 'q';
    }
    fen[idx++] = ' ';

    int ep = board->epsquare;
    if (ep == -1) {
        fen[idx++] = '-';
    } else {
        fen[idx++] = 'a' + (ep & 7);
        fen[idx++] = '0' + (8 - (ep >> 3));
    }
    fen[idx++] = ' ';

    int half = board->halfmoves;
    int full = board->fullmoves;
    idx += sprintf(fen + idx, "%d %d", half, full);
    fen[idx] = '\0';
}

static bool parse_fen(Position *board, const char *fen_str) {
    // Zero the whole board first so any field fenRead doesn't explicitly
    // touch (padding, flags, etc.) can't carry stale/garbage stack data
    // into is_position_valid() and cause spurious, silent rejection loops.
    memset(board, 0, sizeof(*board));

    char fen_copy[256];
    strncpy(fen_copy, fen_str, sizeof(fen_copy) - 1);
    fen_copy[sizeof(fen_copy) - 1] = '\0';

    char *tokens[6] = {NULL};
    int count = 0;
    char *saveptr;
    char *token = strtok_r(fen_copy, " ", &saveptr);
    while (token && count < 6) {
        tokens[count++] = token;
        token = strtok_r(NULL, " ", &saveptr);
    }

    if (count < 4) return false;
    if (count < 5) tokens[4] = "";
    if (count < 6) tokens[5] = "";

    fenRead(board, tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);
    return true;
}

/* -------------------------------------------------------------------- */
/* Book support (OpenBench genfens "book <None|path>" argument)          */
/* -------------------------------------------------------------------- */

static char **book_lines = NULL;
static int book_line_count = 0;

// Loads FEN/EPD lines from `path` into book_lines. A path of NULL or
// "None" is treated as "no book" and leaves book_line_count at 0.
// Always reports the number of lines found, matching the reference
// genfens output format ("info string found N book lines"), even when
// that number is zero, so tooling that parses stdout has a consistent
// line to look for regardless of whether a book was supplied.
static void load_book(const char *path) {
    book_lines = NULL;
    book_line_count = 0;

    if (path && strcmp(path, "None") != 0) {
        FILE *f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "info string genfens: failed to open book '%s'\n", path);
        } else {
            size_t capacity = 1024;
            book_lines = malloc(capacity * sizeof(*book_lines));

            char linebuf[512];
            while (fgets(linebuf, sizeof(linebuf), f)) {
                // Strip trailing newline / carriage return.
                size_t len = strlen(linebuf);
                while (len > 0 && (linebuf[len - 1] == '\n' || linebuf[len - 1] == '\r'))
                    linebuf[--len] = '\0';

                // Skip blank / whitespace-only lines.
                char *p = linebuf;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0') continue;

                if (book_line_count >= (int)capacity) {
                    capacity *= 2;
                    char **grown = realloc(book_lines, capacity * sizeof(*book_lines));
                    if (!grown) {
                        fprintf(stderr, "info string genfens: book realloc failure\n");
                        break;
                    }
                    book_lines = grown;
                }

                book_lines[book_line_count++] = strdup(p);
            }
            fclose(f);
        }
    }

    fprintf(stderr, "info string found %d book lines\n", book_line_count);
    fflush(stderr);
}

static void free_book(void) {
    for (int i = 0; i < book_line_count; i++) {
        free(book_lines[i]);
    }
    free(book_lines);
    book_lines = NULL;
    book_line_count = 0;
}

/* -------------------------------------------------------------------- */

static uint16_t datagen_iterative_deepening(Position *board, stopConditions *stop) {
    uint16_t best_move_so_far = 0;
    int prev_score = 0;
    int aspiration_delta = 25;
    const int ASPIRATION_MAX_DELTA = 500;

    for (int depth = 1; depth <= MAX_DEPTH; depth++) {
        if (stop->soft_nodes > 0 && stop->nodes >= stop->soft_nodes)
            break;

        if (stop->depth > 0 && depth > stop->depth)
            break;

        stop->seldepth = 0;

        PVLine pv = {0};
        searchOutput out;
        SearchStack no_excl = {0};


        int alpha, beta;
        bool first_attempt = (depth == 1);
        if (first_attempt) {
            alpha = -MATE_SCORE;
            beta  =  MATE_SCORE;
        } else {
            alpha = prev_score - aspiration_delta;
            beta  = prev_score + aspiration_delta;
        }

        int delta = aspiration_delta;
        int research_count = 0;
        const int MAX_RESEARCH = 5;

        while (1) {
            pv.length = 0;

            out = search(board, depth, 0, alpha, beta, stop, &pv,&no_excl);

            if (stop->stop)
                break;

            if (out.score > alpha && out.score < beta)
                break;

            if (out.score <= alpha) {
                alpha = out.score - delta;
                if (alpha < -MATE_SCORE) alpha = -MATE_SCORE;
            } else if (out.score >= beta) {
                beta = out.score + delta;
                if (beta > MATE_SCORE) beta = MATE_SCORE;
            }

            delta *= 2;
            if (delta > ASPIRATION_MAX_DELTA) {
                alpha = -MATE_SCORE;
                beta  =  MATE_SCORE;
            }

            research_count++;
            if (research_count >= MAX_RESEARCH) {
                alpha = -MATE_SCORE;
                beta  =  MATE_SCORE;
                pv.length = 0;
                out = search(board, depth, 0, alpha, beta, stop, &pv,&no_excl);
                break;
            }
        }

        if (stop->stop)
            break;

        prev_score = out.score;

        if (out.move != 0) {
            best_move_so_far = out.move;
        }
    }

    return best_move_so_far;
}

// Builds one opening: if a book was supplied, picks a random book line
// (a FEN or EPD line) and uses it directly. Otherwise falls back to the
// original random-walk generation from the startpos. Returns false if
// no valid opening could be produced within OPENING_GEN_TIMEOUT_MS.
static bool generate_opening(Position *board) {
    long long opening_search_start = get_time_ms();

    while (get_time_ms() - opening_search_start < OPENING_GEN_TIMEOUT_MS) {
        if (book_line_count > 0) {
            int line_idx = rng_uniform(book_line_count);
            if (!parse_fen(board, book_lines[line_idx])) {
                continue;
            }
            if (is_position_valid(board)) {
                return true;
            }
            continue;
        }

        if (!parse_fen(board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")) {
            continue;
        }

        int opening_plies = 4 + rng_uniform(7);
        bool valid_opening = true;
        for (int p = 0; p < opening_plies; p++) {
            MoveList moves;
            moves.offset = 0;
            legalMoveGen(board, &moves);
            if (moves.offset == 0) {
                valid_opening = false;
                break;
            }
            int move_idx = rng_uniform(moves.offset);
            makeMove(board, &moves, move_idx);
        }

        if (valid_opening && is_position_valid(board)) {
            return true;
        }
    }

    return false;
}

void datagen_genfens(int argc, char **argv) {
    int N = 0;
    uint64_t seed = 0;
    uint64_t soft_nodes = 5000;
    char *book_path = NULL; // NULL / "None" => no book

    char *tokens[64];
    int token_count = 0;

    for (int i = 1; i < argc; i++) {
        char *copy = strdup(argv[i]);
        char *saveptr;
        char *tok = strtok_r(copy, " ", &saveptr);
        while (tok && token_count < 64) {
            tokens[token_count++] = strdup(tok);
            tok = strtok_r(NULL, " ", &saveptr);
        }
        free(copy);
    }

    for (int i = 0; i < token_count; i++) {
        if (strcmp(tokens[i], "genfens") == 0 && i + 1 < token_count) {
            N = atoi(tokens[i + 1]);
        } else if (strcmp(tokens[i], "seed") == 0 && i + 1 < token_count) {
            seed = strtoull(tokens[i + 1], NULL, 10);
        } else if (strcmp(tokens[i], "soft_nodes") == 0 && i + 1 < token_count) {
            soft_nodes = strtoull(tokens[i + 1], NULL, 10);
        } else if (strcmp(tokens[i], "book") == 0 && i + 1 < token_count) {
            book_path = tokens[i + 1]; // still owned by tokens[]; copy before freeing tokens
        }
    }

    // load_book() strdup's whatever it needs, so it's safe to free tokens[] after this.
    load_book(book_path);

    for (int i = 0; i < token_count; i++) {
        free(tokens[i]);
    }

    rng_seed(seed);
    int generated = 0;

    // Heap-allocate instead of a 128KB stack array. This buffer sat live
    // across every recursive search() call at every ply of the game;
    // combined with per-frame search locals it could plausibly exhaust
    // a thread's stack (especially the smaller default on Windows),
    // causing a hard crash rather than a clean error.
    char (*game_fens)[256] = malloc(300 * sizeof(*game_fens));
    if (!game_fens) {
        fprintf(stderr, "info string genfens: allocation failure\n");
        free_book();
        return;
    }

    while (generated < N) {
        Position board;

        if (!generate_opening(&board)) {
            fprintf(stderr,
                "info string genfens: timed out generating a valid opening, aborting\n");
            break;
        }

        int fen_count = 0;
        bool game_finished = false;

        while (!game_finished && fen_count < 300) {
            fen_from_position(&board, game_fens[fen_count]);
            fen_count++;

            MoveList moves;
            moves.offset = 0;
            legalMoveGen(&board, &moves);
            if (moves.offset == 0) {
                game_finished = true;
                break;
            }

            stopConditions stop = {0};
            stop.start_time = get_time_ms();
            stop.soft_nodes = soft_nodes;
            stop.max_nodes  = soft_nodes * 4;
            stop.max_time   = SEARCH_MAX_TIME_MS;  // wall-clock safety net, independent of node counting
            stop.nodes      = 0;
            stop.stop       = 0;

            uint16_t best_move = datagen_iterative_deepening(&board, &stop);

            if (best_move == 0) {
                game_finished = true;
                break;
            }

            int chosen_idx = -1;
            for (unsigned int i = 0; i < moves.offset; i++) {
                if (moves.movelist[i] == best_move) {
                    chosen_idx = (int)i;
                    break;
                }
            }

            if (chosen_idx == -1) {
                chosen_idx = 0;
            }

            makeMove(&board, &moves, chosen_idx);

            if (board.halfmoves >= 100) {
                game_finished = true;
                break;
            }
        }

        for (int i = 0; i < fen_count && generated < N; i++) {
            printf("info string genfens %s\n", game_fens[i]);
            fflush(stdout);
            generated++;
        }
    }

    free(game_fens);
    free_book();
}