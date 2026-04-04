CC = gcc
CFLAGS = -g -O3 -flto -march=native
LDFLAGS =
TARGET = crustik

SRCS = main.c play.c lmath.c eval.c uci.c fen.c search.c zobrist.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) $(ARGS)

.PHONY: all clean run