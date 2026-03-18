AVRDUDE   = sudo avrdude
DUDEFLAGS = -c linuxgpio -p m8

LFUSE = 0xE4
HFUSE = 0xDA

BOOTLOADER = bootloader/bootloader.hex
FIRMWARE   = firmware/firmware.hex

.PHONY: all flash-all build fuses check-fuses clean

all: build

build:
	$(MAKE) -C bootloader
	$(MAKE) -C firmware

check-fuses:
	@echo "Reading fuses..."
	@LFUSE_READ=$$($(AVRDUDE) $(DUDEFLAGS) -U lfuse:r:-:h 2>/dev/null | tr -d '[:space:]' | tr '[:upper:]' '[:lower:]'); \
	 HFUSE_READ=$$($(AVRDUDE) $(DUDEFLAGS) -U hfuse:r:-:h 2>/dev/null | tr -d '[:space:]' | tr '[:upper:]' '[:lower:]'); \
	 LFUSE_EXP=$$(echo "$(LFUSE)" | tr '[:upper:]' '[:lower:]'); \
	 HFUSE_EXP=$$(echo "$(HFUSE)" | tr '[:upper:]' '[:lower:]'); \
	 echo "  lfuse: expected $$LFUSE_EXP, got $$LFUSE_READ"; \
	 echo "  hfuse: expected $$HFUSE_EXP, got $$HFUSE_READ"; \
	 if [ "$$LFUSE_READ" != "$$LFUSE_EXP" ] || [ "$$HFUSE_READ" != "$$HFUSE_EXP" ]; then \
	   echo "Fuses incorrect, programming..."; \
	   $(AVRDUDE) $(DUDEFLAGS) \
	     -U lfuse:w:$(LFUSE):m \
	     -U hfuse:w:$(HFUSE):m; \
	 else \
	   echo "Fuses OK"; \
	 fi

flash-all: build check-fuses
	$(AVRDUDE) $(DUDEFLAGS) \
		-U flash:w:$(BOOTLOADER):i \
		-U flash:w:$(FIRMWARE):i

fuses:
	$(AVRDUDE) $(DUDEFLAGS) \
		-U lfuse:w:$(LFUSE):m \
		-U hfuse:w:$(HFUSE):m

clean:
	$(MAKE) -C bootloader clean
	$(MAKE) -C firmware clean
