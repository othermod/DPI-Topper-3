DTC = dtc
DTC_FLAGS = -@ -Hepapr -I dts -O dtb

OVERLAYS = topper3-audio-21bit \
           topper3-audio-21bit-pi5

DTBO_FILES = $(OVERLAYS:=.dtbo)

all: $(DTBO_FILES)

%.dtbo: %.dts
	$(DTC) $(DTC_FLAGS) -o $@ $<

clean:
	rm -f $(DTBO_FILES)

.PHONY: all clean
