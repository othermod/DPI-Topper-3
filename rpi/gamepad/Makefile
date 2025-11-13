KERNEL_DIR ?= /usr/src/linux-headers-$(shell uname -r)
PWD := $(shell pwd)
MODULE_NAME = topper3-gamepad
DTB_NAME = $(MODULE_NAME).dtbo
DTS_NAME = $(MODULE_NAME).dts

# Default paths for installation
MODULE_INSTALL_DIR = /lib/modules/$(shell uname -r)/kernel/drivers/input
OVERLAY_INSTALL_DIR = /boot/overlays

obj-m += $(MODULE_NAME).o

# Check for kernel headers before proceeding
define check_kernel_headers
	@if [ ! -d "$(KERNEL_DIR)" ]; then \
		echo "ERROR: Kernel headers not found in $(KERNEL_DIR)"; \
		echo "Please install kernel headers using:"; \
		echo "    sudo apt update"; \
		echo "    sudo apt install raspberrypi-kernel-headers"; \
		exit 1; \
	fi
endef

all: check_headers module dtb

check_headers:
	$(call check_kernel_headers)

module: check_headers
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

dtb:
	dtc -@ -I dts -O dtb -o $(DTB_NAME) $(DTS_NAME)

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f $(DTB_NAME)

install: all
	@if [ "$(shell id -u)" != "0" ]; then \
		echo "You must be root to install. Please run with sudo."; \
		exit 1; \
	fi
	install -d $(MODULE_INSTALL_DIR)
	install -d $(OVERLAY_INSTALL_DIR)
	install -m 644 $(MODULE_NAME).ko $(MODULE_INSTALL_DIR)
	install -m 644 $(DTB_NAME) $(OVERLAY_INSTALL_DIR)
	depmod -a
	@echo ""
	@echo "Installation complete. To enable the driver:"
	@echo "1. Add the following line to /boot/config.txt:"
	@echo "   dtoverlay=topper3-gamepad"
	@echo "2. Reboot your Raspberry Pi"
	@echo ""

uninstall:
	@if [ "$(shell id -u)" != "0" ]; then \
		echo "You must be root to uninstall. Please run with sudo."; \
		exit 1; \
	fi
	rm -f $(MODULE_INSTALL_DIR)/$(MODULE_NAME).ko
	rm -f $(OVERLAY_INSTALL_DIR)/$(DTB_NAME)
	depmod -a
	@echo ""
	@echo "Uninstallation complete. Don't forget to remove the dtoverlay line from /boot/config.txt"
	@echo ""

.PHONY: all check_headers module dtb clean install uninstall