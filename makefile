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
	tt.c \
	uci.c \
	zobrist.c \
	datagen.c

OBJS := $(SRCS:.c=.o)

# Standard
STD := -std=c2x

EVALFILE := quant256hl.bin
EVALDEF  := -DEVALFILE=\"$(EVALFILE)\"

ifeq ($(OS),Windows_NT)
	DEFS :=
	EXE_SUFFIX := .exe
else
	DEFS := -D_POSIX_C_SOURCE=200809L
	EXE_SUFFIX :=
endif

EXE := $(EXE)$(EXE_SUFFIX)

# Warnings
WARN := -Wall -Wextra -Wshadow -Wpedantic

NATIVE ?= 0
ifeq ($(NATIVE),1)
	ARCH := -march=native -mtune=native
else
	ARCH :=
endif

# Optimizations
OPT := -O3 -DNDEBUG -flto $(ARCH)

CFLAGS := $(STD) $(DEFS) $(WARN) $(OPT) $(EVALDEF)
LDFLAGS := -flto
LDLIBS := -lm

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bench: $(EXE)
	./$(EXE) bench

debug: clean
	$(MAKE) CFLAGS="$(STD) $(DEFS) $(WARN) -O0 -g3 $(EVALDEF)" LDFLAGS="" LDLIBS="$(LDLIBS)"

sanitize: clean
	$(MAKE) CFLAGS="$(STD) $(DEFS) $(WARN) -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer $(EVALDEF)" \
		LDFLAGS="-fsanitize=address,undefined" \
		LDLIBS="$(LDLIBS)"

pgo-build: clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-generate" LDFLAGS="$(LDFLAGS) -fprofile-generate" LDLIBS="$(LDLIBS)"
	./$(EXE) bench
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-use -fprofile-correction" LDFLAGS="$(LDFLAGS) -fprofile-use" LDLIBS="$(LDLIBS)"

clean:
	rm -f $(OBJS) $(EXE)

.PHONY: all clean bench debug sanitize pgo-build