CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0`

SRC = src/main.c src/config.c src/util.c src/storage.c src/daemon.c src/toggle_ipc.c src/ui.c
OBJ = $(SRC:.c=.o)

all: rcopy

rcopy: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

install: rcopy
	mkdir -p $(HOME)/.local/bin
	cp rcopy $(HOME)/.local/bin/rcopy

clean:
	rm -f src/*.o rcopy

.PHONY: all install clean
