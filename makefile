EXE ?= crustik

CC ?= gcc

SRCS := \
	main.c \
	play.c \
	lmath.c \
	eval.c \
	uci.c \
	fen.c \
	search.c \
	zobrist.c \
	tt.c \
	bench.c \
	text.c \
	magics.c \
	params.c \
	ordermoves.c

OBJS := $(SRCS:.c=.o)

EVALFILE ?= quant512hl.bin
EVALDEF  := -DEVALFILE=\"$(EVALFILE)\"

TARGET_WINDOWS ?= 0
ifeq ($(OS),Windows_NT)
	TARGET_WINDOWS := 1
endif

ifeq ($(TARGET_WINDOWS),1)
	EXE_SUFFIX  := .exe
	# Win32 CreateThread is used directly - no winpthreads dependency.
	THREADFLAGS :=
else
	EXE_SUFFIX  :=
	# Required on BOTH the compile and the link line.
	THREADFLAGS := -pthread
endif

TARGET := $(EXE)$(EXE_SUFFIX)

WARN := -Wall -Wextra -Wshadow

NATIVE ?= 1
ifeq ($(NATIVE),1)
	ARCH := -march=native
else
	ARCH :=
endif

LTO ?= -flto=auto


BASEFLAGS := $(WARN) $(THREADFLAGS) $(EVALDEF)

CFLAGS  ?= $(BASEFLAGS) -DNDEBUG -O3 $(LTO) $(ARCH)
LDFLAGS ?= $(LTO) $(THREADFLAGS)
LDLIBS  := -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bench: $(TARGET)
	./$(TARGET) bench

debug: clean
	$(MAKE) CFLAGS="$(BASEFLAGS) -O0 -g3" LDFLAGS="$(THREADFLAGS)"

sanitize: clean
	$(MAKE) CFLAGS="$(BASEFLAGS) -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="$(THREADFLAGS) -fsanitize=address,undefined"

tsan: clean
	$(MAKE) LTO="" NATIVE=0 \
		CFLAGS="$(BASEFLAGS) -O1 -g3 -fsanitize=thread -fno-omit-frame-pointer" \
		LDFLAGS="$(THREADFLAGS) -fsanitize=thread"

pgo: clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-generate" LDFLAGS="$(LDFLAGS) -fprofile-generate"
	./$(TARGET) bench
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-use -fprofile-correction" LDFLAGS="$(LDFLAGS) -fprofile-use"

clean:
	rm -f $(OBJS) $(TARGET)

distclean: clean
	rm -f *.gcda *.gcno

.PHONY: all clean distclean bench debug sanitize tsan pgo
