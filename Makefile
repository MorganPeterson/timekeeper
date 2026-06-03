CC := cc

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
MAN1DIR ?= $(MANDIR)/man1
MAN5DIR ?= $(MANDIR)/man5

BIN := timekeeper
SRC := src/timekeeper.c

WARN := -Wall -Wextra -Werror
STD := -std=c11

CFLAGS_COMMON := $(STD) $(WARN)

.PHONY: all clean test size speed debug strip install uninstall

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS_COMMON) -O2 -flto $(SRC) -o $(BIN)

size: $(SRC)
	$(CC) $(CFLAGS_COMMON) -Os -ffunction-sections -fdata-sections $(SRC) \
		-Wl,--gc-sections -s -o $(BIN)

speed: $(SRC)
	$(CC) $(CFLAGS_COMMON) -O3 -march=native -flto $(SRC) -o $(BIN)

debug: $(SRC)
	$(CC) $(CFLAGS_COMMON) -O0 -g3 -fsanitize=address,undefined $(SRC) -o $(BIN)

strip: $(BIN)
	strip $(BIN)

test: $(BIN)
	./test.sh

clean:
	rm -f $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) "$(BINDIR)/$(BIN)"
	install -Dm644 man/timekeeper.1 "$(MAN1DIR)/timekeeper.1"
	install -Dm644 man/timekeeper-config.5 "$(MAN5DIR)/timekeeper-config.5"

uninstall:
	rm -f /usr/local/bin/$(BIN)
	rm -f "$(MAN1DIR)/timekeeper.1"
	rm -f "$(MAN5DIR)/timekeeper-config.5"