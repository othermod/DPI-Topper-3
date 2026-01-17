# Raspberry Pi Kernel Device Tree Build
KERNEL_REPO = https://github.com/raspberrypi/linux.git
KERNEL_DIR = /tmp/rpi-linux
KERNEL_BUILD_THREADS = 6

# Build configuration
KERNEL := kernel8
ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-
DEFCONFIG := bcm2711_defconfig

# Our custom overlay files
OVERLAYS = topper3-display-21bit \
           topper3-display-21bit-noi2c \
           topper3-display-24bit \
           topper3-display-24bit-noi2c

DTS_FILES = $(OVERLAYS:=.dts)
DTBO_FILES = $(OVERLAYS:=.dtbo)

.PHONY: all clean help check-kernel-deps kernel-clone kernel-config kernel-build

help:
	@echo "Device Tree Overlay Build Targets:"
	@echo "  make all     - Clone, configure, and build overlays"
	@echo "  make clean   - Remove kernel directory"

# Check dependencies needed for kernel build
check-kernel-deps:
	@echo "Checking for kernel build dependencies..."
	@command -v git >/dev/null 2>&1 || { echo "Error: git not found"; exit 1; }
	@command -v bc >/dev/null 2>&1 || { echo "Error: bc not found"; exit 1; }
	@command -v bison >/dev/null 2>&1 || { echo "Error: bison not found"; exit 1; }
	@command -v flex >/dev/null 2>&1 || { echo "Error: flex not found"; exit 1; }
	@command -v $(CROSS_COMPILE)gcc >/dev/null 2>&1 || { echo "Error: $(CROSS_COMPILE)gcc not found"; exit 1; }
	@echo "All dependencies found"

# Clone the kernel repository if it doesn't exist
kernel-clone: check-kernel-deps
	@if [ -d "$(KERNEL_DIR)" ]; then \
		echo "Kernel directory already exists at $(KERNEL_DIR)"; \
	else \
		echo "Cloning Raspberry Pi Linux kernel to $(KERNEL_DIR) (shallow clone)..."; \
		git clone --depth=1 $(KERNEL_REPO) $(KERNEL_DIR); \
	fi

# Configure kernel
kernel-config: kernel-clone
	@echo "Configuring kernel for Raspberry Pi..."
	@cd $(KERNEL_DIR) && KERNEL=$(KERNEL) make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $(DEFCONFIG)

# Build device tree overlays
kernel-build: kernel-config
	@echo "Building device tree overlays..."
	@OVERLAY_DIR="$(KERNEL_DIR)/arch/$(ARCH)/boot/dts/overlays"; \
	GENERIC_DTS="$$OVERLAY_DIR/vc4-kms-dpi-generic-overlay.dts"; \
	GENERIC_DTBO="$$OVERLAY_DIR/vc4-kms-dpi-generic.dtbo"; \
	CURRENT_DIR=$$(pwd); \
	echo "Running initial dtbs build..."; \
	cd $(KERNEL_DIR) && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) dtbs; \
	for overlay in $(OVERLAYS); do \
		echo ""; \
		echo "Building $$overlay.dtbo..."; \
		cp "$$CURRENT_DIR/$$overlay.dts" "$$GENERIC_DTS"; \
		cd $(KERNEL_DIR) && make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) dtbs; \
		cp "$$GENERIC_DTBO" "$$OVERLAY_DIR/$$overlay.dtbo"; \
		cp "$$OVERLAY_DIR/$$overlay.dtbo" "$$CURRENT_DIR/"; \
		ls -lh "$$CURRENT_DIR/$$overlay.dtbo"; \
	done; \
	echo ""; \
	echo "Build complete."

# Setup and build (complete process)
all: kernel-build

# Remove kernel directory
clean:
	@if [ -d "$(KERNEL_DIR)" ]; then \
		echo "Removing kernel directory..."; \
		rm -rf $(KERNEL_DIR); \
		echo "Cleaned"; \
	fi
