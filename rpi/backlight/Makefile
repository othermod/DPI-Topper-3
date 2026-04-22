CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11 -fomit-frame-pointer -ffast-math -pipe
LDFLAGS = -s

BINDIR = bin

all: $(BINDIR)/backlight-gamepad-armhf $(BINDIR)/backlight-gamepad-arm64 \
     $(BINDIR)/backlight-basic-armhf $(BINDIR)/backlight-basic-arm64 \
     $(BINDIR)/backlight-ctl-armhf $(BINDIR)/backlight-ctl-arm64

$(BINDIR)/backlight-gamepad-armhf: backlight-gamepad.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-gamepad-arm64: backlight-gamepad.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-basic-armhf: backlight-basic.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-basic-arm64: backlight-basic.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-ctl-armhf: backlight-ctl.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-ctl-arm64: backlight-ctl.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR):
	mkdir -p $@

native: backlight-gamepad backlight-basic backlight-ctl

backlight-gamepad: backlight-gamepad.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-basic: backlight-basic.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-ctl: backlight-ctl.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f backlight-gamepad backlight-basic backlight-ctl
	rm -rf $(BINDIR)

.PHONY: all native clean
