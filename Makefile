CC ?= cc
ODIR=obj

LIBS != pkg-config --cflags --libs x11 xkbfile libpulse dbus-1

CFLAGS ?= -Wall -Wextra -O3 -march=native -flto -pipe

all: sucktus

sucktus: sucktus.c
	$(CC) -o $@ $(CFLAGS) $^ $(LIBS) -lm

.PHONY: clean test all

clean:
	rm -f sucktus

test:
	@echo "Test is not available yet!"
