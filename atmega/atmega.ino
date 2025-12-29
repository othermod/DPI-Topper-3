#include <Wire.h>
#include <EEPROM.h>
#include "config.h"

struct SystemState {
  uint8_t debounceCount[16];
  bool dispPressed;
  uint8_t currentJoystick;
  bool crcEnabled;
};

struct i2cStructure {
  uint16_t buttons;  // Combined button states
  uint8_t joyLX;
  uint8_t joyLY;
  uint8_t joyRX;
  uint8_t joyRY;
  union {
    struct Status {
      uint8_t brightness : 3;  // Bits 0-2: Display brightness level (0-7)
      bool reserved1 : 1;      // Bit 3: Reserved for future use
      bool reserved2 : 1;      // Bit 4: Reserved for future use
      bool reserved3 : 1;      // Bit 5: Reserved for future use
      bool reserved4 : 1;      // Bit 6: Reserved for future use
      bool reserved5 : 1;      // Bit 7: Reserved for future use
    } status;
    uint8_t systemStatus;      // Access as full byte
  };
  uint16_t crc16;
};

// Global state declarations
SystemState state;
i2cStructure i2cdata;
volatile byte rxData[4];
volatile bool pendingCommand = false;
unsigned long lastUpdateTime = 0;
uint16_t crcTable[256];

// Loop control
#define UPDATE_INTERVAL_REACHED currentTime - lastUpdateTime >= LOOP_MS

void initGPIOs() {
  // Set all pins as inputs with pull-ups enabled
  DDRB = 0b00000000;  // All inputs
  DDRD = 0b00000000;  // All inputs
  PORTB = 0b11111111; // All pull-ups enabled
  PORTD = 0b11111111; // All pull-ups enabled

  /* Pin Configuration Summary:
     PORTB (0-7): Buttons 0-7 (IP, PU)
     PORTD (0-7): Buttons 8-15 (IP, PU)
     LCD_1W (C3): LCD backlight control (OP, DL)
     BTN_DISP (C2): Display brightness button (IP, PU)
  */

  setPinAsOutput(LCD_1W);
  setPinLow(LCD_1W);
  setPinAsInput(BTN_DISP);
  setPinHigh(BTN_DISP);
}

void generateCRCTable() {
  const uint16_t poly = 0x1021;

  for (uint16_t i = 0; i < 256; i++) {
    uint16_t crc = i << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ poly;
      } else {
        crc = crc << 1;
      }
    }
    crcTable[i] = crc;
  }
}

void calculateCRC() {
  uint16_t crc = 0xFFFF;
  const uint8_t* data = (const uint8_t*)&i2cdata;

  // Process first 7 bytes of i2cdata structure (2 for buttons, 4 for joysticks, 1 for status)
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t tableIndex = (crc >> 8) ^ data[i];
    crc = (crc << 8) ^ crcTable[tableIndex];
  }

  i2cdata.crc16 = crc;
}

void readEEPROM() {
  uint8_t brightness = EEPROM.read(EEPROM_BRIGHT_ADDR);

  // Handle freshly flashed ATmega (0xFF EEPROM values)
  if (brightness > 7) {
    i2cdata.status.brightness = BRIGHTNESS_DEFAULT;
  } else {
    i2cdata.status.brightness = brightness;
  }
}

void writeBrightnessToEEPROM() {
  EEPROM.update(EEPROM_BRIGHT_ADDR, i2cdata.status.brightness);
}

void setBrightness() {
  // Verify display is currently enabled
  if (!readPin(LCD_1W)) {
    return;
  }

  byte bytesToSend[] = {LCD_ADDR, i2cdata.status.brightness * 4 + 1};

  noInterrupts();
  for (int byte = 0; byte < 2; byte++) {
    // Start condition
    delayMicroseconds(T_START);

    // Send each bit MSB first
    for (int i = 7; i >= 0; i--) {
      bool bit = bytesToSend[byte] & (1 << i);
      setPinLow(LCD_1W);
      delayMicroseconds(bit ? T_L_HB : T_L_LB);
      setPinHigh(LCD_1W);
      delayMicroseconds(bit ? T_H_HB : T_H_LB);
    }

    // End of byte sequence
    setPinLow(LCD_1W);
    delayMicroseconds(T_EOS);
    setPinHigh(LCD_1W);
  }

  interrupts();
  // Always update the eeprom when brightness is set
  writeBrightnessToEEPROM();
}

void disableDisplay() {
  // Verify that the display isn't already off
  if (!readPin(LCD_1W)) {
    return;
  }

  // Fade from current brightness down to minimum
  uint8_t currentRaw = i2cdata.status.brightness * 4 + 1;

  for (uint8_t rawBrightness = currentRaw; rawBrightness >= 1; rawBrightness--) {
    byte bytesToSend[] = {LCD_ADDR, rawBrightness};

    noInterrupts();
    for (int byte = 0; byte < 2; byte++) {
      delayMicroseconds(T_START);

      for (int i = 7; i >= 0; i--) {
        bool bit = bytesToSend[byte] & (1 << i);
        setPinLow(LCD_1W);
        delayMicroseconds(bit ? T_L_HB : T_L_LB);
        setPinHigh(LCD_1W);
        delayMicroseconds(bit ? T_H_HB : T_H_LB);
      }

      setPinLow(LCD_1W);
      delayMicroseconds(T_EOS);
      setPinHigh(LCD_1W);
    }
    interrupts();

    delay(20);
  }

  setPinLow(LCD_1W);
}

void enableDisplay() {
  // Verify that the display isn't already on
  if (readPin(LCD_1W)) {
    return;
  }

  // TPS61160 initialization sequence
  setPinLow(LCD_1W);
  delayMicroseconds(T_OFF);
  setPinHigh(LCD_1W);
  delayMicroseconds(150);
  setPinLow(LCD_1W);
  delayMicroseconds(300);
  setPinHigh(LCD_1W);

  // Fade from off to target brightness
  uint8_t targetRaw = i2cdata.status.brightness * 4 + 1;
  for (uint8_t rawBrightness = 1; rawBrightness <= targetRaw; rawBrightness++) {
    byte bytesToSend[] = {LCD_ADDR, rawBrightness};

    noInterrupts();
    for (int byte = 0; byte < 2; byte++) {
      delayMicroseconds(T_START);

      for (int i = 7; i >= 0; i--) {
        bool bit = bytesToSend[byte] & (1 << i);
        setPinLow(LCD_1W);
        delayMicroseconds(bit ? T_L_HB : T_L_LB);
        setPinHigh(LCD_1W);
        delayMicroseconds(bit ? T_H_HB : T_H_LB);
      }

      setPinLow(LCD_1W);
      delayMicroseconds(T_EOS);
      setPinHigh(LCD_1W);
    }
    interrupts();

    delay(20);
  }
}

void readJoysticks() {
  // Read one joystick axis per loop, cycling through all 4
  switch(state.currentJoystick) {
    case 0:
      i2cdata.joyLX = analogRead(JOY_LX) >> 2;
      break;
    case 1:
      i2cdata.joyLY = analogRead(JOY_LY) >> 2;
      break;
    case 2:
      i2cdata.joyRX = analogRead(JOY_RX) >> 2;
      break;
    case 3:
      i2cdata.joyRY = analogRead(JOY_RY) >> 2;
      break;
  }

  // Increment to next joystick, wrapping around to 0 after 3
  state.currentJoystick = (state.currentJoystick + 1) & 0b00000011;
}

void checkDisplayButton() {
  if (!readPin(BTN_DISP)) {
    state.dispPressed = true;
  } else if (state.dispPressed) {
    state.dispPressed = false;

    // Check if display is currently off
    if (!readPin(LCD_1W)) {
      // Display is OFF - re-enable at current brightness
      enableDisplay();
    } else {
      // Display is ON - increment brightness normally
      i2cdata.status.brightness++; // Increment to next brightness. Valid brightness levels are 0-7. This will roll over to 0 because it is only 3 bits
      setBrightness();
    }
  }
}

void processI2CCommand() {
  switch (rxData[0]) {
    case I2C_CMD_BRIGHT:
      if (rxData[1] == I2C_BRIGHT_DISABLE) {
        // Special value: disable display
        disableDisplay();
      } else if (rxData[1] == I2C_BRIGHT_ENABLE) {
        // Special value: enable display at previous brightness
        if (!readPin(LCD_1W)) {
          enableDisplay();
        }
      } else if (rxData[1] <= 7) {
        i2cdata.status.brightness = rxData[1];

        // Check if display is currently off
        if (!readPin(LCD_1W)) {
          enableDisplay();  // Was off, enable at new brightness
        } else {
          setBrightness();  // Already on, just change brightness
        }
      }
      break;

    case I2C_CMD_CRC:
      state.crcEnabled = rxData[1];
      break;
  }
}

void readButtons() {
  // Read and invert in one operation
  uint16_t pressed = ~((PIND << 8) | PINB);
  uint16_t button_state = 0;

  for (uint8_t i = 0; i < 16; i++) {
    // Check if currently pressed
    if (pressed & (1 << i)) {
      state.debounceCount[i] = BTN_DEBOUNCE_DURATION;
      button_state |= (1 << i);  // Set bit immediately if pressed
    }
    // Not pressed but still in duration window
    else if (state.debounceCount[i] > 0) {
      state.debounceCount[i]--;
      button_state |= (1 << i);
    }
  }

  i2cdata.buttons = button_state;
}

void onRequest() {
  if (state.crcEnabled) {
    calculateCRC();
  }

  Wire.write((const uint8_t*)&i2cdata, sizeof(i2cdata));
}

void onReceive(int numBytes) {
  // Read up to 4 bytes
  for (int i = 0; i < 4 && Wire.available(); i++) {
    rxData[i] = Wire.read();
  }

  // Drain any extra bytes
  while (Wire.available()) {
    Wire.read();
  }

  pendingCommand = true;
}

void checkForIncomingI2CCommand() {
  if (pendingCommand) {
    processI2CCommand();
    pendingCommand = false;
  }
}

void normalModeFunctions() {
  readJoysticks();
  readButtons();
  checkDisplayButton();
}

void setup() {
  initGPIOs();
  generateCRCTable();

  // Initialize state
  state.currentJoystick = 0;
  state.dispPressed = false;
  state.crcEnabled = true;  // CRC enabled by default

  readEEPROM();
  enableDisplay();

  Wire.begin(I2C_ADDR);
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);
}

void loop() {
  checkForIncomingI2CCommand();  // Process any pending I2C commands immediately

  unsigned long currentTime = millis();
  if (UPDATE_INTERVAL_REACHED) {
    lastUpdateTime = currentTime;
    normalModeFunctions();
  }
}
