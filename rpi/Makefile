COMPONENTS = display audio gamepad backlight touch
INSTALL_DIR = /boot/firmware/overlays
BIN_INSTALL_DIR = /usr/local/bin

.PHONY: all cross clean install uninstall display audio gamepad backlight touch check-deps check-cross-deps

check-deps:
	@echo "Checking for required build tools..."
	@command -v dtc >/dev/null 2>&1 || { echo "Error: dtc (device-tree-compiler) not found"; echo "Install with: sudo apt-get install device-tree-compiler"; exit 1; }
	@command -v gcc >/dev/null 2>&1 || { echo "Error: gcc not found"; echo "Install with: sudo apt-get install build-essential"; exit 1; }
	@echo "All dependencies satisfied"

check-cross-deps:
	@echo "Checking for cross-compilation tools..."
	@command -v dtc >/dev/null 2>&1 || { echo "Error: dtc (device-tree-compiler) not found"; echo "Install with: sudo apt-get install device-tree-compiler"; exit 1; }
	@command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1 || { echo "Error: arm-linux-gnueabihf-gcc not found"; echo "Install with: sudo apt-get install gcc-arm-linux-gnueabihf"; exit 1; }
	@command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 || { echo "Error: aarch64-linux-gnu-gcc not found"; echo "Install with: sudo apt-get install gcc-aarch64-linux-gnu"; exit 1; }
	@echo "All cross-compilation dependencies satisfied"

all: check-deps display audio gamepad backlight touch

display: check-deps
	$(MAKE) -C display

audio: check-deps
	$(MAKE) -C audio

gamepad: check-deps
	$(MAKE) -C gamepad

backlight: check-deps
	$(MAKE) -C backlight

touch: check-deps
	$(MAKE) -C touch

cross: check-cross-deps
	$(MAKE) -C display cross
	$(MAKE) -C audio cross
	$(MAKE) -C gamepad cross
	$(MAKE) -C backlight cross

clean:
	$(MAKE) -C display clean
	$(MAKE) -C audio clean
	$(MAKE) -C gamepad clean
	$(MAKE) -C backlight clean
	$(MAKE) -C touch clean

install: all
	@if [ "$$(id -u)" != "0" ]; then \
		echo "Error: install requires root. Please run with sudo."; \
		exit 1; \
	fi
	install -d $(INSTALL_DIR)
	install -m 644 display/*.dtbo $(INSTALL_DIR)
	install -m 644 audio/*.dtbo $(INSTALL_DIR)
	install -d $(BIN_INSTALL_DIR)
	install -m 755 gamepad/gamepad $(BIN_INSTALL_DIR)
	install -m 755 backlight/backlight $(BIN_INSTALL_DIR)
	install -m 755 backlight/backlight-minimal $(BIN_INSTALL_DIR)
	install -m 755 backlight/backlight-ctl $(BIN_INSTALL_DIR)
	@echo "Installation complete"

uninstall:
	@if [ "$$(id -u)" != "0" ]; then \
		echo "Error: uninstall requires root. Please run with sudo."; \
		exit 1; \
	fi
	rm -f $(INSTALL_DIR)/topper3-display-*.dtbo
	rm -f $(INSTALL_DIR)/topper3-audio-*.dtbo
	rm -f $(BIN_INSTALL_DIR)/gamepad
	rm -f $(BIN_INSTALL_DIR)/backlight
	rm -f $(BIN_INSTALL_DIR)/backlight-minimal
	rm -f $(BIN_INSTALL_DIR)/backlight-ctl
	@echo "Uninstall complete"
