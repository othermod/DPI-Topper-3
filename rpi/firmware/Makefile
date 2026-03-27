CC       = gcc
CFLAGS   = -Wall -Wextra -Wshadow -O2
TARGET   = update_firmware
SRC      = update_firmware.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
