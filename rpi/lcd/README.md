# LCD Display Setup

This guide provides instructions for enabling LCD display output on the Raspberry Pi DPI Topper 3.

## Choose Your Mode

**24-bit mode:**
- Full color display

**21-bit mode:**
- Frees GPIOs 4, 12, and 20

## Installation

### 1. Copy the Overlay File

Insert your Raspberry Pi SD card into your computer and copy the appropriate overlay file to the overlays folder:

**For 24-bit mode:** Copy `vc4-kms-dpi-topper3-24bit.dtbo`  
**For 21-bit mode:** Copy `vc4-kms-dpi-topper3-21bit.dtbo`

**Location:**
- Older Pi OS: `boot/overlays/`
- Newer Pi OS (Bookworm): `bootfs/overlays/`

### 2. Edit config.txt

Open `config.txt` (located in the `boot` or `bootfs` folder) and add one of the following lines:

**For 24-bit mode:**
```
dtoverlay=vc4-kms-dpi-topper3-24bit
```

**For 21-bit mode:**
```
dtoverlay=vc4-kms-dpi-topper3-21bit
```

### 3. Reboot

Save the file, eject the SD card, and boot your Raspberry Pi.
