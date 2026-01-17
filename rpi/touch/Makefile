TARGET = touchscreen
SRC = touchscreen.c

CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET)

.PHONY: clean
