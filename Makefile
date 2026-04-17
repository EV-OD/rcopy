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
	mkdir -p $(HOME)/.config/systemd/user
	mkdir -p $(HOME)/.local/bin
	install -m 755 rcopy $(HOME)/.local/bin/rcopy.new
	mv -f $(HOME)/.local/bin/rcopy.new $(HOME)/.local/bin/rcopy
	cp scripts/rcopy.service $(HOME)/.config/systemd/user/rcopy.service
	cp scripts/rcopy-picker.service $(HOME)/.config/systemd/user/rcopy-picker.service
	systemctl --user daemon-reload
	systemctl --user enable --now rcopy.service rcopy-picker.service

install-bin: rcopy
	mkdir -p $(HOME)/.local/bin
	install -m 755 rcopy $(HOME)/.local/bin/rcopy.new
	mv -f $(HOME)/.local/bin/rcopy.new $(HOME)/.local/bin/rcopy

install-startup:
	mkdir -p $(HOME)/.config/systemd/user
	cp scripts/rcopy.service $(HOME)/.config/systemd/user/rcopy.service
	cp scripts/rcopy-picker.service $(HOME)/.config/systemd/user/rcopy-picker.service
	systemctl --user daemon-reload
	systemctl --user enable --now rcopy.service rcopy-picker.service

disable-startup:
	systemctl --user disable --now rcopy.service rcopy-picker.service
	rm -f $(HOME)/.config/systemd/user/rcopy.service
	rm -f $(HOME)/.config/systemd/user/rcopy-picker.service
	systemctl --user daemon-reload

clean:
	rm -f src/*.o rcopy

.PHONY: all install install-bin install-startup disable-startup clean
