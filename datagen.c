// genfens.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "play.h"
#include "fen.h"
#include "lmath.h"

#define DEFAULT_PLIES 8
#define MAX_GENERATION_ATTEMPTS 256

/* ------------------------------------------------------------------------- */
/* Custom case-insensitive string comparison (portable)                      */
/* ------------------------------------------------------------------------- */

static int str_iequal(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* ------------------------------------------------------------------------- */
/* SplitMix64 RNG (exactly matching Rust implementation)                     */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint64_t state;
} SplitMix64;

static SplitMix64 splitmix64_new(uint64_t seed)
{
    SplitMix64 rng;
    rng.state = seed;
    return rng;
}

static uint64_t splitmix64_next_u64(SplitMix64 *rng)
{
    rng->state = rng->state + 0x9e3779b97f4a7c15ULL;
    uint64_t z = rng->state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static size_t splitmix64_index(SplitMix64 *rng, size_t len)
{
    if (len == 0) return 0;
    return (size_t)(splitmix64_next_u64(rng) % (uint64_t)len);
}

/* ------------------------------------------------------------------------- */
/* PlyRange (exactly matching Rust implementation)                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t min;
    uint32_t max;
} PlyRange;

static PlyRange ply_range_exact(uint32_t plies)
{
    PlyRange range;
    range.min = plies;
    range.max = plies;
    return range;
}

static uint32_t ply_range_sample(PlyRange range, SplitMix64 *rng)
{
    if (range.min == range.max) {
        return range.min;
    }
    return range.min + (uint32_t)splitmix64_index(rng, (size_t)(range.max - range.min + 1));
}

/* ------------------------------------------------------------------------- */
/* GenfensArgs                                                               */
/* ------------------------------------------------------------------------- */

typedef struct {
    size_t count;
    uint64_t seed;
    char *book;
    PlyRange plies;
} GenfensArgs;

/* ------------------------------------------------------------------------- */
/* Parsing functions                                                         */
/* ------------------------------------------------------------------------- */

static int parse_nonzero_usize(const char *raw, const char *label, size_t *out)
{
    char *end = NULL;
    unsigned long long value = strtoull(raw, &end, 10);
    
    if (end == raw || *end != '\0') {
        fprintf(stderr, "invalid genfens %s: %s\n", label, raw);
        return 0;
    }
    
    if (value == 0) {
        fprintf(stderr, "genfens %s must be greater than zero\n", label);
        return 0;
    }
    
    *out = (size_t)value;
    return 1;
}

static int parse_ply_range(const char *raw, PlyRange *out)
{
    const char *dash = strchr(raw, '-');
    
    if (!dash) {
        // Exact plies
        char *end = NULL;
        unsigned long plies = strtoul(raw, &end, 10);
        
        if (end == raw || *end != '\0') {
            fprintf(stderr, "invalid plies value: %s\n", raw);
            return 0;
        }
        
        if (plies == 0) {
            fprintf(stderr, "plies must be greater than zero\n");
            return 0;
        }
        
        *out = ply_range_exact((uint32_t)plies);
        return 1;
    }
    
    // Range: min-max
    char min_str[32];
    char max_str[32];
    size_t min_len = (size_t)(dash - raw);
    
    if (min_len >= sizeof(min_str)) {
        fprintf(stderr, "invalid plies range: %s\n", raw);
        return 0;
    }
    
    strncpy(min_str, raw, min_len);
    min_str[min_len] = '\0';
    strncpy(max_str, dash + 1, sizeof(max_str) - 1);
    max_str[sizeof(max_str) - 1] = '\0';
    
    char *end = NULL;
    unsigned long min = strtoul(min_str, &end, 10);
    if (end == min_str || *end != '\0') {
        fprintf(stderr, "invalid minimum plies value: %s\n", raw);
        return 0;
    }
    
    end = NULL;
    unsigned long max = strtoul(max_str, &end, 10);
    if (end == max_str || *end != '\0') {
        fprintf(stderr, "invalid maximum plies value: %s\n", raw);
        return 0;
    }
    
    if (min == 0 || max == 0 || min > max) {
        fprintf(stderr, "invalid plies range: %s\n", raw);
        return 0;
    }
    
    out->min = (uint32_t)min;
    out->max = (uint32_t)max;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* FEN generation                                                            */
/* ------------------------------------------------------------------------- */

static char piece_from_bitboards(Position *b, int sq)
{
    uint64_t bb = 1ULL << sq;
    int side;

    if (b->color[0] & bb) side = 0;
    else if (b->color[1] & bb) side = 1;
    else return 0;

    if (b->pieces[0] & bb) return side == 0 ? 'P' : 'p';
    if (b->pieces[1] & bb) return side == 0 ? 'B' : 'b';
    if (b->pieces[2] & bb) return side == 0 ? 'N' : 'n';
    if (b->pieces[3] & bb) return side == 0 ? 'R' : 'r';
    if (b->pieces[4] & bb) return side == 0 ? 'Q' : 'q';
    if (b->pieces[5] & bb) return side == 0 ? 'K' : 'k';
    return 0;
}

static void generate_fen_string(Position *b, char *out, size_t out_size)
{
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
        if (empty) {
            pos += snprintf(out + pos, out_size - pos, "%d", empty);
        }
        if (rank != 7) {
            pos += snprintf(out + pos, out_size - pos, "/");
        }
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
    if (!castle) {
        pos += snprintf(out + pos, out_size - pos, "-");
    }

    pos += snprintf(out + pos, out_size - pos, " ");

    if (b->epsquare == -1) {
        pos += snprintf(out + pos, out_size - pos, "-");
    } else {
        int sq = b->epsquare;
        pos += snprintf(out + pos, out_size - pos, "%c%d", 'a' + (sq & 7), 8 - (sq >> 3));
    }

    snprintf(out + pos, out_size - pos, " %u %u", (unsigned)b->halfmoves, (unsigned)b->fullmoves);
}

/* ------------------------------------------------------------------------- */
/* Opening book loading                                                      */
/* ------------------------------------------------------------------------- */

typedef struct {
    char **fens;
    size_t count;
} StartPositions;

static int load_start_positions(const char *book, StartPositions *starts)
{
    starts->fens = NULL;
    starts->count = 0;
    
    if (str_iequal(book, "none")) {
        starts->fens = malloc(sizeof(char *));
        if (!starts->fens) return 0;
        starts->fens[0] = strdup("startpos");
        if (!starts->fens[0]) {
            free(starts->fens);
            starts->fens = NULL;
            return 0;
        }
        starts->count = 1;
        return 1;
    }
    
    FILE *file = fopen(book, "r");
    if (!file) {
        fprintf(stderr, "failed to open book: %s\n", book);
        return 0;
    }
    
    char line[1024];
    size_t capacity = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline
        line[strcspn(line, "\r\n")] = '\0';
        
        // Skip empty lines and comments
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;
        
        if (starts->count == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 16;
            char **new_fens = realloc(starts->fens, new_capacity * sizeof(char *));
            if (!new_fens) {
                fclose(file);
                return 0;
            }
            starts->fens = new_fens;
            capacity = new_capacity;
        }
        
        // Parse book line
        char fen_line[1024];
        
        // Try to parse as full FEN (6 fields)
        char *tokens[16];
        int token_count = 0;
        char *saveptr = NULL;
        char *token = strtok_r(trimmed, " \t", &saveptr);
        
        while (token && token_count < 16) {
            tokens[token_count++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        
        if (token_count >= 6) {
            // Check if fields 5 and 6 are numbers (halfmove and fullmove)
            char *end1 = NULL;
            char *end2 = NULL;
            strtoul(tokens[4], &end1, 10);
            strtoul(tokens[5], &end2, 10);
            
            if (end1 != tokens[4] && *end1 == '\0' && 
                end2 != tokens[5] && *end2 == '\0') {
                // Full FEN
                snprintf(fen_line, sizeof(fen_line), "%s %s %s %s %s %s",
                         tokens[0], tokens[1], tokens[2], tokens[3],
                         tokens[4], tokens[5]);
            } else {
                // Try EPD format
                uint32_t halfmove = 0;
                uint32_t fullmove = 1;
                
                for (int i = 0; i < token_count - 1; i++) {
                    if (strcmp(tokens[i], "hmvc") == 0) {
                        char *end = NULL;
                        char *val = tokens[i + 1];
                        // Remove trailing semicolon
                        size_t len = strlen(val);
                        if (len > 0 && val[len - 1] == ';') {
                            val[len - 1] = '\0';
                        }
                        unsigned long hm = strtoul(val, &end, 10);
                        if (end != val && *end == '\0') {
                            halfmove = (uint32_t)hm;
                        }
                    } else if (strcmp(tokens[i], "fmvn") == 0) {
                        char *end = NULL;
                        char *val = tokens[i + 1];
                        // Remove trailing semicolon
                        size_t len = strlen(val);
                        if (len > 0 && val[len - 1] == ';') {
                            val[len - 1] = '\0';
                        }
                        unsigned long fm = strtoul(val, &end, 10);
                        if (end != val && *end == '\0') {
                            fullmove = (uint32_t)fm;
                            if (fullmove == 0) fullmove = 1;
                        }
                    }
                }
                
                snprintf(fen_line, sizeof(fen_line), "%s %s %s %s %u %u",
                         tokens[0], tokens[1], tokens[2], tokens[3],
                         halfmove, fullmove);
            }
        } else if (token_count >= 4) {
            // Just 4 fields, use default halfmove and fullmove
            snprintf(fen_line, sizeof(fen_line), "%s %s %s %s 0 1",
                     tokens[0], tokens[1], tokens[2], tokens[3]);
        } else {
            fprintf(stderr, "malformed book line: %s\n", trimmed);
            continue;
        }
        
        starts->fens[starts->count] = strdup(fen_line);
        if (starts->fens[starts->count]) {
            starts->count++;
        }
    }
    
    fclose(file);
    
    if (starts->count == 0) {
        fprintf(stderr, "book contains no usable positions: %s\n", book);
        return 0;
    }
    
    return 1;
}

static void free_start_positions(StartPositions *starts)
{
    if (!starts) return;
    for (size_t i = 0; i < starts->count; i++) {
        free(starts->fens[i]);
    }
    free(starts->fens);
    starts->fens = NULL;
    starts->count = 0;
}

/* ------------------------------------------------------------------------- */
/* Engine interface                                                          */
/* ------------------------------------------------------------------------- */

static int set_start(Position *board, const char *start)
{
    if (strcmp(start, "startpos") == 0) {
        // Set to start position
        memset(board, 0, sizeof(*board));
        fenRead(board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                "w", "KQkq", "-", "0", "1");
        board->color[2] = board->color[0] | board->color[1];
        return 1;
    }
    
    // Parse FEN
    char temp[512];
    strncpy(temp, start, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *tok[6];
    int t = 0;
    char *p = strtok(temp, " ");
    
    while (p && t < 6) {
        tok[t++] = p;
        p = strtok(NULL, " ");
    }
    
    if (t < 6) return 0;
    
    fenRead(board, tok[0], tok[1], tok[2], tok[3], tok[4], tok[5]);
    board->color[2] = board->color[0] | board->color[1];
    return 1;
}

static int is_game_ongoing(Position *board)
{
    MoveList legal = {0};
    legalMoveGen(board, &legal);
    
    // Check if there are legal moves
    if (legal.offset == 0) {
        return 0;
    }
    
    // Check for 50-move rule
    if (board->halfmoves >= 100) {
        return 0;
    }
    
    return 1;
}

static void play_random_plies(Position *board, uint32_t plies, SplitMix64 *rng)
{
    for (uint32_t i = 0; i < plies; i++) {
        if (!is_game_ongoing(board)) {
            return;
        }
        
        MoveList legal_moves = {0};
        legalMoveGen(board, &legal_moves);
        
        if (legal_moves.offset == 0) {
            return;
        }
        
        size_t index = splitmix64_index(rng, legal_moves.offset);
        makeMove(board, &legal_moves, (int)index);
    }
}

/* ------------------------------------------------------------------------- */
/* generate_opening (exactly matching Rust implementation)                   */
/* ------------------------------------------------------------------------- */

static int generate_opening(
    StartPositions *starts,
    PlyRange plies,
    SplitMix64 *rng,
    char *out_fen,
    size_t out_size)
{
    for (int attempt = 0; attempt < MAX_GENERATION_ATTEMPTS; attempt++) {
        Position board;
        memset(&board, 0, sizeof(board));
        
        const char *start = starts->fens[splitmix64_index(rng, starts->count)];
        
        if (!set_start(&board, start)) {
            continue;
        }
        
        play_random_plies(&board, ply_range_sample(plies, rng), rng);
        
        if (is_game_ongoing(&board)) {
            generate_fen_string(&board, out_fen, out_size);
            return 1;
        }
    }
    
    fprintf(stderr, "failed to generate a non-terminal opening\n");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Main genfens command                                                      */
/* ------------------------------------------------------------------------- */

void datagen_genfens(int argc, char **argv)
{
    // Parse command: genfens <count> seed <seed> book <book> [plies=<plies>]
    if (argc < 7 || strcmp(argv[1], "genfens") != 0) {
        fprintf(stderr, "malformed genfens command\n");
        fprintf(stderr, "usage: genfens <count> seed <seed> book <book> [plies=<plies>]\n");
        return;
    }
    
    GenfensArgs args;
    memset(&args, 0, sizeof(args));
    args.plies = ply_range_exact(DEFAULT_PLIES);
    
    // Parse count
    if (!parse_nonzero_usize(argv[2], "count", &args.count)) {
        return;
    }
    
    // Check for "seed" keyword
    if (strcmp(argv[3], "seed") != 0) {
        fprintf(stderr, "expected seed after genfens count\n");
        return;
    }
    
    // Parse seed
    char *end = NULL;
    args.seed = strtoull(argv[4], &end, 10);
    if (end == argv[4] || *end != '\0') {
        fprintf(stderr, "invalid genfens seed: %s\n", argv[4]);
        return;
    }
    
    // Check for "book" keyword
    if (strcmp(argv[5], "book") != 0) {
        fprintf(stderr, "expected book after genfens seed\n");
        return;
    }
    
    // Get book name
    args.book = strdup(argv[6]);
    if (!args.book) {
        fprintf(stderr, "out of memory\n");
        return;
    }
    
    // Parse optional arguments (starting from argv[7])
    for (int i = 7; i < argc; i++) {
        if (strncmp(argv[i], "plies=", 6) == 0) {
            if (!parse_ply_range(argv[i] + 6, &args.plies)) {
                free(args.book);
                return;
            }
        } else if (argv[i][0] != '\0') {
            fprintf(stderr, "unknown genfens argument: %s\n", argv[i]);
            free(args.book);
            return;
        }
    }
    
    // Load start positions
    StartPositions starts;
    if (!load_start_positions(args.book, &starts)) {
        free(args.book);
        return;
    }
    
    // Initialize RNG
    SplitMix64 rng = splitmix64_new(args.seed);
    
    // Generate openings
    char fen[1024];
    for (size_t i = 0; i < args.count; i++) {
        if (generate_opening(&starts, args.plies, &rng, fen, sizeof(fen))) {
            printf("info string genfens %s\n", fen);
        }
    }
    
    // Cleanup
    free_start_positions(&starts);
    free(args.book);
}