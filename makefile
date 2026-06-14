# Engine name
EXE := crustik

# Compiler
CC := gcc

# Sources
SRCS := \
	main.c \
	bench.c \
	eval.c \
	fen.c \
	lmath.c \
	play.c \
	search.c \
	text.c \
	uci.c \
	zobrist.c

OBJS := $(SRCS:.c=.o)

# Standard (C23)
STD := -std=c2x

# Required POSIX/GNU extensions for clock_gettime, etc.
DEFS := -D_POSIX_C_SOURCE=200809L

# Warnings
WARN := -Wall -Wextra -Wshadow -Wpedantic

# Optimizations
OPT := -O3 -DNDEBUG -flto -march=native -mtune=native

CFLAGS := $(STD) $(DEFS) $(WARN) $(OPT)
LDFLAGS := -flto


# Default target
all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bench: $(EXE)
	./$(EXE) bench

debug: clean
	$(MAKE) CFLAGS="$(STD) $(DEFS) $(WARN) -O0 -g3"

sanitize: clean
	$(MAKE) CFLAGS="$(STD) $(DEFS) $(WARN) \
		-O1 -g3 \
		-fsanitize=address,undefined \
		-fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address,undefined"

pgo-build: clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-generate" LDFLAGS="$(LDFLAGS) -fprofile-generate"
	./$(EXE) bench
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-use -fprofile-correction" LDFLAGS="$(LDFLAGS) -fprofile-use"

clean:
	rm -f $(OBJS) $(EXE)

.PHONY: all clean bench debug sanitize pgo-build