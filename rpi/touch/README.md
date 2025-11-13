# Touch Panel Setup

This guide provides instructions for enabling touch input on the Raspberry Pi DPI Topper 3.

## Installation

### Option 1: Overlay
This works well when using most recent operating systems, and it allows all the gestures (drag, pinch, long press, etc.) to work.

#### 1. Edit config.txt

Open `config.txt` (located in the `boot` or `bootfs` folder) and add the following line:

```
dtoverlay=edt-ft5406,i2c1,invx,invy
```

#### 2. Reboot

Save the file, eject the SD card, and boot your Raspberry Pi.
