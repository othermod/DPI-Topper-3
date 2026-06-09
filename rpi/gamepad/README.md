Gamepad driver and button mapper for the [DPI-Topper-3](https://github.com/othermod/DPI-Topper-3).

Reads button and joystick state from the Topper's ATmega over I2C and exposes a virtual PS3 DualShock 3 controller via Linux uinput. SDL and RetroArch recognize it automatically via GUID `030000004c0500006802000011810000`.

---

## Requirements

- Linux with uinput support (`/dev/uinput`)
- I2C enabled on `/dev/i2c-1`
- Topper ATmega firmware running at I2C address `0x30`
- Cross-compiler toolchain for your target architecture:

```
sudo apt install gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu
```

---

## Building

```
make 32    # Pi Zero, Pi 1, Pi 2
make 64    # Pi 3, Pi 4, Pi 5
```

Produces `32/gamepad` + `32/mapper` or `64/gamepad` + `64/mapper`.

---

## Setup: generating your map string

The Topper exposes up to 16 button inputs across two GPIO ports. Because buttons can be wired to any pin, the driver needs a map string that tells it which input bit corresponds to which controller button.

Run the mapper to generate this string interactively:

```
./mapper
```

The mapper walks through all 17 PS3 buttons in order. For each one, press the corresponding button on your hardware. Press **Enter** to skip any button that doesn't exist on your controller. At the end it prints the map string and the exact command to run the driver.

Example session:

```
[ 1/17] D-pad Up               (ENTER to skip)
        Press the button now...
        Detected bit 3. Release the button... ok.
        Bit 3 -> D-pad Up
        Keep? [y=yes  n=skip  r=redo]: y
        Mapped.

[ 2/17] D-pad Down             (ENTER to skip)
        Press the button now...
```

```
Map string: ---D---G--------

Run the driver with:
  gamepad --map ---D---G--------
```

---

## Running the driver

```
gamepad --map <string> [options]
```

`--map` is required. All other options are optional.

| Option | Default | Description |
|---|---|---|
| `--map <string>` | — | 16-character button mapping string (required) |
| `--joysticks <0-2>` | `2` | Number of analog sticks to read |
| `--min <0-255>` | `40` | Stick axis minimum value |
| `--max <0-255>` | `215` | Stick axis maximum value |
| `--deadzone <0-100>` | `20` | Stick axis deadzone (flat) |
| `--autocenter` | off | Sample stick positions at startup as center point |

---

## Map string format

The map string is 16 characters, one per input bit (bit 0 at position 0, bit 15 at position 15). Each character names the PS3 button that bit drives. Use `-` to leave a bit unmapped.

| Character | PS3 button |
|---|---|
| `0` | Cross / A |
| `1` | Circle / B |
| `2` | Triangle / Y |
| `3` | Square / X |
| `4` | L1 |
| `5` | R1 |
| `6` | L2 (also drives trigger axis) |
| `7` | R2 (also drives trigger axis) |
| `8` | Back / Select |
| `9` | Start |
| `A` | Guide / PS |
| `B` | L3 |
| `C` | R3 |
| `D` | D-pad Up |
| `E` | D-pad Down |
| `F` | D-pad Left |
| `G` | D-pad Right |
| `-` | Unmapped |

**Example:** up/down/left/right wired to bits 0-3, A button on bit 7:

```
gamepad --map DEFG---0--------
```

Bits mapped to L2 (`6`) or R2 (`7`) emit both the button event and the corresponding trigger axis (`ABS_Z` / `ABS_RZ`), so they work correctly in both SDL and RetroArch.

---

## I2C wire format

The driver reads a 9-byte packet from the Topper on each poll (≈60 Hz):

| Bytes | Field |
|---|---|
| 0-1 | Button bitfield, little-endian (bit 0-7 = PORTB, bit 8-15 = PORTD) |
| 2 | Left stick X |
| 3 | Left stick Y |
| 4 | Right stick X |
| 5 | Right stick Y |
| 6 | Status flags (brightness, display state, etc.) |
| 7-8 | CRC-16-CCITT over bytes 0-6, little-endian |

CRC is always validated. Packets that fail are silently discarded and the driver retries immediately.
