LIBS=libpng xcb

INSTALL_DIR=$(HOME)/.local/bin

CFLAGS=-Wall -Wextra -Wpedantic -std=c90 -pedantic \
	   $(shell for lib in $(LIBS); do pkg-config --cflags $$lib; done) \
	   $(EXTRA)

CLIBS=$(shell for lib in $(LIBS); do pkg-config --libs $$lib; done)

OPT=-O3 -s -flto -march=native -mtune=native

DEBUG=-Og -g -DDEBUG

CSOURCE=main.c

.PHONY: all
all: draw-png debug

debug: $(CSOURCE)
	$(CC) $(CFLAGS) $(CLIBS) $(DEBUG) $^ -o $@

draw-png: $(CSOURCE)
	$(CC) $(CFLAGS) $(CLIBS) $(OPT) $^ -o $@

.PHONY: install
install: draw-png
	install ./draw-png $(INSTALL_DIR)/sub-cropper

.PHONY: clean
clean:
	rm draw-png debug
