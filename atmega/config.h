// System Configuration
#define LOOP_MS 1       // Main loop interval in ms

// Button configuration macros
#define BTN_DEBOUNCE_DURATION 10  // Buttons will remain "pressed" for this many loops

#define I2C_ADDR 0x30
#define I2C_IDLE_TRIGGER 200    // I2C timeout in number of NORMAL_MODE_LOOP_MS loops

// EEPROM Addresses
#define EEPROM_BRIGHT_ADDR 0

// Brightness Configuration
#define BRIGHTNESS_DEFAULT 4 // 0-7 are valid
