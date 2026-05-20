CC := gcc
BIN := timekeeper
SRC := timekeeper.c

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
	install -Dm755 $(BIN) /usr/local/bin/$(BIN)

uninstall:
	rm -f /usr/local/bin/$(BIN)
