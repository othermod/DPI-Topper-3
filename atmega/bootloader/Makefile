TARGET     = bootloader
MCU        = atmega8
F_CPU      = 8000000UL
BOOT_START = 0x1C00
BINDIR     = bin

CC         = avr-gcc
OBJCOPY    = avr-objcopy
SIZE       = avr-size

CFLAGS     = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBOOTLOADER_START=$(BOOT_START) \
             -Os -Wall -Wextra -ffreestanding -fno-inline-small-functions \
             -fpack-struct -fshort-enums -ffunction-sections -fdata-sections

LDFLAGS    = -mmcu=$(MCU) -nostartfiles -Wl,--section-start=.text=$(BOOT_START) \
             -Wl,--gc-sections -Wl,--relax

all: $(BINDIR)/$(TARGET).hex

$(BINDIR)/$(TARGET).elf: bootloader.c | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BINDIR)/$(TARGET).hex: $(BINDIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@PROGSIZE=$$($(SIZE) $< | tail -1 | awk '{print $$1}'); \
	 DATASIZE=$$($(SIZE) $< | tail -1 | awk '{print $$2+$$3}'); \
	 PROG_FREE=$$((1024 - $$PROGSIZE)); \
	 DATA_FREE=$$((1024 - $$DATASIZE)); \
	 echo "Flash:   $$PROGSIZE bytes used of 1024 for bootloader [$$PROG_FREE bytes free]"; \
	 echo "RAM:     $$DATASIZE bytes used of 1024 [$$DATA_FREE bytes free]"

$(BINDIR):
	mkdir -p $@

clean:
	rm -rf $(BINDIR)

.PHONY: all clean
