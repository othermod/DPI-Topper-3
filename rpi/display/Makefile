DTC = dtc
GCC = gcc

# Directory containing kernel dt-bindings headers
KDIR = /home/user/linux

# Source and output files
SRCS = topper3-display-24bit.dts \
       topper3-display-24bit-noi2c.dts \
       topper3-display-21bit.dts \
       topper3-display-21bit-noi2c.dts

OBJS = $(patsubst %.dts,%.dtbo,$(SRCS))

# Include path for dt-bindings headers
INCLUDE = -I$(KDIR)/include

# Preprocessor flags for DTS files
CPPFLAGS = -nostdinc -x assembler-with-cpp -undef -D__DTS__ $(INCLUDE)

# dtc flags for Raspberry Pi overlays
DTCFLAGS = -@ -Hepapr -I dts -O dtb -Wno-unit_address_vs_reg

.PHONY: all clean

all: $(OBJS)

%.dtbo: %.dts
	$(GCC) $(CPPFLAGS) -E $< | $(DTC) $(DTCFLAGS) -o $@ -

clean:
	rm -f $(OBJS)
