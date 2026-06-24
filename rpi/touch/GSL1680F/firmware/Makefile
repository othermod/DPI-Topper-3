# Builds the silead.c kernel driver firmware blob from the vendor's
# C header. Requires python3 and gsl_fw_compile.py in the same directory.
#
# Usage:
#   make            # builds the .fw file
#   make clean      # removes it

PYTHON    ?= python3
COMPILER  := gsl_fw_compile.py

FW_SRC := gslX680_311_5_F.h
FW_BIN := gsl1680-buydisplay-ertft050-6.fw

.PHONY: all clean

all: $(FW_BIN)

$(FW_BIN): $(FW_SRC) $(COMPILER)
	$(PYTHON) $(COMPILER) $< $@

clean:
	rm -f $(FW_BIN)
