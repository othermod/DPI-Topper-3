# Gamepad Setup

This guide provides instructions for enabling gamepad input support on the Raspberry Pi DPI Topper 3. The gamepad driver reads 16 digital buttons and 4 analog axes (two joysticks) from the ATmega microcontroller via I2C and exposes them as a standard Linux input device (emulating a PS3 controller).

## Prerequisites

- Raspberry Pi OS with kernel headers installed:
  ```bash
  sudo apt update
  sudo apt install raspberrypi-kernel-headers
  ```

## Installation

### 1. Build the Driver

Navigate to the directory containing `topper3-gamepad.c` and `topper3-gamepad.dts`, and build the module and device tree overlay:

```bash
make
```

This compiles `topper3-gamepad.ko` (kernel module) and `topper3-gamepad.dtbo` (overlay).

### 2. Install the Driver

Install as root:

```bash
sudo make install
```

This copies the module to `/lib/modules/$(uname -r)/kernel/drivers/input/` and the overlay to `/boot/overlays/`, then runs `depmod -a`.

### 3. Enable the Overlay

Edit your boot configuration file and add the overlay:

**For newer Raspberry Pi OS (Bookworm and later):**
```bash
sudo nano /boot/firmware/config.txt
```

**For older Raspberry Pi OS:**
```bash
sudo nano /boot/config.txt
```

Add this line:
```
dtoverlay=topper3-gamepad
```

Save and exit (Ctrl+O, Enter, Ctrl+X in nano).

### 4. Reboot

Reboot your Raspberry Pi:

```bash
sudo reboot
```

## Verification

After reboot, check if the gamepad is detected:

```bash
ls /dev/input/js*
```

You should see `/dev/input/js0` (or js1, js2, etc. if you have other gamepads).

To test the gamepad, install and run `jstest`:

```bash
sudo apt install joystick
jstest /dev/input/js0
```

Press buttons and move joysticks to see the values change.

## Uninstall

To remove the driver:

```bash
sudo make uninstall
```

Then remove `dtoverlay=topper3-gamepad` from your boot config file and reboot.
