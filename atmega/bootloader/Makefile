TARGET     = bootloader
MCU        = atmega8
F_CPU      = 8000000UL

# ATmega8: 512 word bootloader section starts at 0x1C00 (byte address)
BOOT_START = 0x1C00

LFUSE      = 0xE4
HFUSE      = 0xDA

AVRDUDE    = sudo avrdude
DUDEFLAGS  = -c linuxgpio -p m8

# Color codes
GREEN  = \033[0;32m
RED    = \033[0;31m
ORANGE = \033[0;33m
NC     = \033[0m

CC         = avr-gcc
OBJCOPY    = avr-objcopy
SIZE       = avr-size
CFLAGS     = -mmcu=$(MCU) \
             -DF_CPU=$(F_CPU) \
             -DBOOTLOADER_START=$(BOOT_START) \
             -Os \
             -Wall \
             -Wextra \
             -ffreestanding \
             -fno-inline-small-functions \
             -fpack-struct \
             -fshort-enums \
             -ffunction-sections \
             -fdata-sections

LDFLAGS    = -mmcu=$(MCU) \
             -nostartfiles \
             -Wl,--section-start=.text=$(BOOT_START) \
             -Wl,--gc-sections \
             -Wl,--relax

.PHONY: all clean flash

all: $(TARGET).hex

$(TARGET).elf: bootloader.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@PROGSIZE=$$($(SIZE) $< | tail -1 | awk '{print $$1}'); \
	 DATASIZE=$$($(SIZE) $< | tail -1 | awk '{print $$2+$$3}'); \
	 PROG_FREE=$$((1024 - $$PROGSIZE)); \
	 DATA_FREE=$$((1024 - $$DATASIZE)); \
	 echo "Flash:   $$PROGSIZE bytes used of 1024 for bootloader [$$PROG_FREE bytes free]"; \
	 echo "RAM:     $$DATASIZE bytes used of 1024 [$$DATA_FREE bytes free]"

flash: $(TARGET).hex
	@if $(AVRDUDE) $(DUDEFLAGS) -U flash:w:$(TARGET).hex:i 2>&1; then \
		printf "$(GREEN)[OK] Bootloader flashed successfully$(NC)\n"; \
		OUTPUT=$$($(AVRDUDE) $(DUDEFLAGS) -n 2>&1); \
		LFUSE_READ=$$(echo "$$OUTPUT" | grep -oP '(?<=L:)[0-9A-Fa-f]+' | tr '[:upper:]' '[:lower:]'); \
		HFUSE_READ=$$(echo "$$OUTPUT" | grep -oP '(?<=H:)[0-9A-Fa-f]+' | tr '[:upper:]' '[:lower:]'); \
		LFUSE_EXP=$$(echo "$(LFUSE)" | sed 's/0x//i' | tr '[:upper:]' '[:lower:]'); \
		HFUSE_EXP=$$(echo "$(HFUSE)" | sed 's/0x//i' | tr '[:upper:]' '[:lower:]'); \
		if [ "$$LFUSE_READ" != "$$LFUSE_EXP" ] || [ "$$HFUSE_READ" != "$$HFUSE_EXP" ]; then \
			printf "$(ORANGE)[NOTICE] Fuses incorrect, programming...$(NC)\n"; \
			$(AVRDUDE) $(DUDEFLAGS) \
				-U lfuse:w:$(LFUSE):m \
				-U hfuse:w:$(HFUSE):m; \
			printf "$(GREEN)[OK] Fuses programmed$(NC)\n"; \
		else \
			printf "$(GREEN)[OK] Fuses correct$(NC)\n"; \
		fi; \
	else \
		printf "$(RED)[FAIL] Bootloader flashing failed$(NC)\n"; \
		exit 1; \
	fi

clean:
	rm -f $(TARGET).elf $(TARGET).hex
