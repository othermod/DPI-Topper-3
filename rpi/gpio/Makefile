CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = gpio

all: $(TARGET)

$(TARGET): gpio.c
	$(CC) $(CFLAGS) -o $@ $<

32: $(TARGET)

64: $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all 32 64 clean
