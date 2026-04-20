// System Configuration
#define LOOP_MS 1       // Main loop interval in ms

// Button configuration macros
#define BTN_DEBOUNCE_DURATION 10  // Buttons will remain "pressed" for this many loops

#define I2C_ADDR 0x30
#define I2C_IDLE_TRIGGER 200    // I2C timeout in number of NORMAL_MODE_LOOP_MS loops

// EEPROM Addresses
#define EEPROM_BRIGHT_ADDR 0
#define EEPROM_DDRB 1
#define EEPROM_PORTB 2
#define EEPROM_DDRD 3
#define EEPROM_PORTD 4

// Brightness Configuration
#define BRIGHTNESS_DEFAULT 4 // 0-7 are valid

// Pin Definitions
#define BTN_DISP C,2
#define LCD_1W C,3

// ADC pins
#define JOY_LX 0
#define JOY_LY 1
#define JOY_RX 6
#define JOY_RY 7

// GPIO Port manipulation macros
#define DDR(p) DDR##p
#define PORT(p) PORT##p
#define PIN(p) PIN##p
#define BIT(n) (1<<n)

// GPIO Pin control macros
#define setOutMode(p,n) DDR(p)|=BIT(n)
#define setInMode(p,n) DDR(p)&=~BIT(n)
#define setHigh(p,n) PORT(p)|=BIT(n)
#define setLow(p,n) PORT(p)&=~BIT(n)
#define getPin(p,n) (PIN(p)&BIT(n))

// GPIO Simplified pin interface macros
#define setPinAsOutput(pin) setOutMode(pin)
#define setPinAsInput(pin) setInMode(pin)
#define setPinHigh(pin) setHigh(pin)
#define setPinLow(pin) setLow(pin)
#define readPin(pin) getPin(pin)

// TPS61160 Backlight EasyScale Protocol
#define LCD_ADDR 0x72

// I2C Command IDs
#define I2C_CMD_BRIGHT 0x10
#define I2C_CMD_CRC 0x20
#define I2C_CMD_GPIO_ALL 0x30
#define I2C_CMD_GPIO_SAVE 0x40
#define I2C_CMD_RESET 0x50

// Firmware Version
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0

// I2C Command Values
#define I2C_BRIGHT_DISABLE 8  // Send this value with I2C_CMD_BRIGHT to disable display
#define I2C_BRIGHT_ENABLE 9   // Send this value with I2C_CMD_BRIGHT to enable display at previous brightness

// LCD timing parameters (microseconds)
#define T_START 10   // Start condition
#define T_EOS 10     // End of sequence
#define T_H_LB 10    // High time, low bit
#define T_H_HB 25    // High time, high bit
#define T_L_LB 25    // Low time, low bit
#define T_L_HB 10    // Low time, high bit
#define T_OFF 3000   // Reset time
