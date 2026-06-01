CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Isrc
LDFLAGS = -lsqlite3 -lpthread

SRCS    = src/main.c src/server.c src/db.c
TARGET  = backend

all: $(TARGET)

$(TARGET): $(SRCS) src/db.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
