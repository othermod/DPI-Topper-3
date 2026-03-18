TARGET   = firmware
MCU      = atmega8
F_CPU    = 8000000UL

# Verify Arduino AVR core is installed before doing anything
ARDUINO_AVR = $(lastword $(sort $(wildcard $(HOME)/.arduino15/packages/arduino/hardware/avr/*)))

ifeq ($(ARDUINO_AVR),)
ifneq ($(MAKECMDGOALS),clean)
$(info )
$(info Arduino AVR core not found. To compile this firmware you need:)
$(info   1. AVR toolchain:)
$(info      sudo apt install gcc-avr binutils-avr avr-libc)
$(info   2. Arduino AVR core source files:)
$(info      curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh)
$(info      arduino-cli core install arduino:avr)
$(info   3. Run make again)
$(info )
$(error Arduino AVR core not found)
endif
endif

CC      = avr-gcc
CXX     = avr-g++
OBJCOPY = avr-objcopy
AR      = avr-gcc-ar
SIZE    = avr-size

AVRDUDE   = sudo avrdude
DUDEFLAGS = -c linuxgpio -p m8

BOOT_START = 7168

CORE_DIR    = $(ARDUINO_AVR)/cores/arduino
WIRE_DIR    = $(ARDUINO_AVR)/libraries/Wire/src
VARIANT_DIR = $(ARDUINO_AVR)/variants/standard
EEPROM_DIR  = $(ARDUINO_AVR)/libraries/EEPROM/src

INC = -I$(CORE_DIR) -I$(WIRE_DIR) -I$(WIRE_DIR)/utility -I$(VARIANT_DIR) -I$(EEPROM_DIR) -I.

CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DARDUINO=10819 \
           -Os -Wall -flto -ffunction-sections -fdata-sections $(INC)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fpermissive

LDFLAGS  = -mmcu=$(MCU) -Wl,--gc-sections -flto -fuse-linker-plugin

CORE_OBJS = $(CORE_DIR)/wiring.o \
            $(CORE_DIR)/wiring_analog.o \
            $(CORE_DIR)/wiring_digital.o \
            $(CORE_DIR)/hooks.o \
            $(CORE_DIR)/main.o

WIRE_OBJS = $(WIRE_DIR)/Wire.o \
            $(WIRE_DIR)/utility/twi.o

.PHONY: all clean install

all: $(TARGET).hex

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

firmware.o: firmware.ino config.h
	$(CXX) $(CXXFLAGS) -x c++ -c -o $@ $<

libcore.a: $(CORE_OBJS)
	$(AR) rcs $@ $^

$(TARGET).elf: firmware.o libcore.a $(WIRE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ firmware.o $(WIRE_OBJS) -L. -lcore

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@PROGSIZE=$$($(SIZE) $< | tail -1 | awk '{print $$1}'); \
	 DATASIZE=$$($(SIZE) $< | tail -1 | awk '{print $$2+$$3}'); \
	 PROG_PCT=$$(($$PROGSIZE * 100 / $(BOOT_START))); \
	 DATA_PCT=$$(($$DATASIZE * 100 / 1024)); \
	 PROG_FREE=$$(($(BOOT_START) - $$PROGSIZE)); \
	 DATA_FREE=$$((1024 - $$DATASIZE)); \
	 BOOT_SIZE=$$((8192 - $(BOOT_START))); \
	 echo "Flash:   8192 bytes total"; \
	 echo "         $$BOOT_SIZE bytes used for bootloader"; \
	 echo "         $$PROGSIZE bytes used of $(BOOT_START) for firmware [$$PROG_FREE bytes free]"; \
	 echo "RAM:     $$DATASIZE bytes used of 1024 [$$DATA_FREE bytes free]"

install: $(TARGET).hex
	$(AVRDUDE) $(DUDEFLAGS) -U flash:w:$<:i

clean:
	rm -f firmware.o libcore.a $(TARGET).elf $(TARGET).hex
	rm -f $(CORE_DIR)/wiring.o $(CORE_DIR)/wiring_analog.o $(CORE_DIR)/wiring_digital.o $(CORE_DIR)/hooks.o $(CORE_DIR)/main.o
	rm -f $(WIRE_DIR)/Wire.o $(WIRE_DIR)/utility/twi.o
