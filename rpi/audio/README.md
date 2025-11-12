# Audio Setup

This guide provides instructions for enabling audio output on the Raspberry Pi DPI Topper 3.

**Note:** Audio requires the LCD to be configured in 21-bit mode. Follow the LCD setup guide and install the `vc4-kms-dpi-topper3-21bit` overlay before proceeding.

## Choose Your Overlay

**For Raspberry Pi Zero/3/4:**
- Use `audremap-topper3.dtbo`

**For Raspberry Pi 5:**
- Use `audremap-topper3-pi5.dtbo`

## Installation

### 1. Copy the Overlay File

Insert your Raspberry Pi SD card into your computer and copy the appropriate overlay file to the overlays folder:

**Location:**
- Older Pi OS: `boot/overlays/`
- Newer Pi OS (Bookworm): `bootfs/overlays/`

### 2. Edit config.txt

Open `config.txt` (located in the `boot` or `bootfs` folder) and add one of the following lines:

**For Raspberry Pi Zero/3/4:**
```
dtoverlay=audremap-topper3
```

**For Raspberry Pi 5:**
```
dtoverlay=audremap-topper3-pi5
```

### 3. Reboot

Save the file, eject the SD card, and boot your Raspberry Pi.
