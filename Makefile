# Native build (dev machine). Yocto builds via the recipe in yocto/ instead.
CC      ?= gcc
PKGS     = libsystemd openssl
CFLAGS  += -std=c11 -Wall -Wextra -Wformat=2 -O2 -g $(shell pkg-config --cflags $(PKGS))
LDLIBS  += $(shell pkg-config --libs $(PKGS))

SRC  = $(wildcard src/*.c)
OBJ  = $(SRC:.c=.o)
BIN  = telemetryd

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: clean
