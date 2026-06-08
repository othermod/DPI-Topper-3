DTC = dtc
DTCFLAGS = -@ -Hepapr -I dts -O dtb

OVERLAYS = topper3-audio-21bit \
           topper3-audio-21bit-pi5

DTBO_FILES = $(OVERLAYS:=.dtbo)

all: $(DTBO_FILES)

%.dtbo: %.dts
	$(DTC) $(DTCFLAGS) -o $@ $<

clean:
	rm -f $(DTBO_FILES)

.PHONY: all clean
