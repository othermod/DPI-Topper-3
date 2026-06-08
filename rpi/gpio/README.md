Command-line utility for reading and configuring the ATmega8's GPIO pins over I2C. Modelled on `raspi-gpio`.

## Pin numbering

The ATmega8 exposes 16 GPIO pins split across two ports:

| GPIO | Port  | Bit |
|------|-------|-----|
| 0-7  | PORTB | 0-7 |
| 8-15 | PORTD | 0-7 |

## Build

```bash
gcc -o gpio gpio.c
```

## Usage

```
gpio get [<pin>|<pin,pin,...>|<start>-<end>]
gpio set <pin>[,<pin>,...] <options>...
gpio set <start>-<end> <options>...
```

### get

```bash
gpio get           # all pins
gpio get 5         # single pin
gpio get 0,3,7     # comma-separated list
gpio get 0-7       # range
```

Output format:

```
GPIO 5: FSEL=0 FUNC=INPUT LEVEL=1 PULL=UP
GPIO 6: FSEL=1 FUNC=OUTPUT LEVEL=0
```

### set

Options can be combined and are applied left to right.

| Option | Effect |
|--------|--------|
| `ip`   | Input |
| `op`   | Output |
| `pu`   | Pull-up (input), or drive high (output) |
| `pn`   | No pull (input), or drive low (output) |
| `dh`   | Drive high — use with `op` |
| `dl`   | Drive low — use with `op` |
| `pd`   | Not supported on ATmega8; ignored with a warning |

```bash
gpio set 5 op           # output, level unchanged
gpio set 5 op dh        # output driving high
gpio set 5 op dl        # output driving low
gpio set 5 ip pu        # input with pull-up
gpio set 5 ip pn        # floating input
gpio set 0,3,7 ip pn    # floating input on pins 0, 3, and 7
gpio set 0-15 ip pn     # floating input on all pins
```

## Notes

- Reads current state before every `set` and modifies only the target pin.
- Changes are not persisted to EEPROM automatically. To save the current GPIO configuration across reboots, send I2C command `0x40` (`I2C_CMD_GPIO_SAVE`).
- I2C device defaults to `/dev/i2c-1`, ATmega address `0x30`.
