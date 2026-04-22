CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11 -fomit-frame-pointer -ffast-math -pipe
LDFLAGS = -s

all: backlight-gamepad-armhf backlight-gamepad-arm64 \
     backlight-basic-armhf backlight-basic-arm64 \
     backlight-ctl-armhf backlight-ctl-arm64

backlight-gamepad-armhf: backlight-gamepad.c
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-gamepad-arm64: backlight-gamepad.c
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-basic-armhf: backlight-basic.c
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-basic-arm64: backlight-basic.c
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-ctl-armhf: backlight-ctl.c
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-ctl-arm64: backlight-ctl.c
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

native: backlight-gamepad backlight-basic backlight-ctl

backlight-gamepad: backlight-gamepad.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-basic: backlight-basic.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

backlight-ctl: backlight-ctl.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f backlight-gamepad-armhf backlight-gamepad-arm64 \
		backlight-basic-armhf backlight-basic-arm64 \
		backlight-ctl-armhf backlight-ctl-arm64 \
		backlight-gamepad backlight-basic backlight-ctl

.PHONY: all native clean
