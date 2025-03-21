#include <Wire.h>
#include <EEPROM.h>
#include "config.h"

struct i2cStructure {
    uint16_t buttons;  // Combined button states
    uint8_t joyLX;
    uint8_t joyLY;
    uint8_t joyRX;
    uint8_t joyRY;
    uint8_t brightness : 3;  // Bits 0-2: Display brightness level (0-7)
      bool reserved1 : 1; 
      bool reserved2 : 1;
      bool reserved3 : 1;
      bool reserved4 : 1;
      bool reserved5 : 1;
    uint16_t crc16;
};

// Global state declarations
i2cStructure i2cdata;
volatile byte rxData[3];
volatile bool pendingCommand = false;
unsigned long lastUpdateTime = 0;
uint8_t button_counters[16];
uint16_t crcTable[256];
uint8_t currentJoystick = 0;

// Loop control
#define UPDATE_INTERVAL_REACHED currentTime - lastUpdateTime >= LOOP_MS

void initGPIOs() {
  // Set all pins as inputs
  DDRB = 0b00000000;
  DDRD = 0b00000000;
  // Enable internal pull-up resistors
  PORTB = 0b11111111;
  PORTD = 0b11111111;
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
  i2cdata.brightness = EEPROM.read(EEPROM_BRIGHT_ADDR);
  if (i2cdata.brightness > 7) {
    i2cdata.brightness = BRIGHTNESS_DEFAULT;
  }
}

void writeBrightnessToEEPROM() {
  EEPROM.update(EEPROM_BRIGHT_ADDR, i2cdata.brightness);
}

void setBrightness() {
    byte bytesToSend[] = {LCD_ADDR,i2cdata.brightness * 4 + 1};
    
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
}

void disableDisplay() {
    setPinLow(LCD_1W);
}

void initBacklight() {
    setPinLow(LCD_1W);
    delayMicroseconds(T_OFF);
    setPinHigh(LCD_1W);
    delayMicroseconds(150);
    setPinLow(LCD_1W);
    delayMicroseconds(300);
    setPinHigh(LCD_1W);
}

void readJoysticks() {
    // Read one joystick axis per loop, cycling through all 4
    switch(currentJoystick) {
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
    currentJoystick = (currentJoystick + 1) & 0b00000011;  // Using bitwise to roll over to 0
}

bool dispPressed = false;
void checkDisplayButton() {
    if (!readPin(BTN_DISP)) {
        dispPressed = true;
    } else if (dispPressed) {
        i2cdata.brightness++; // increment to next brightness. valid brightness levels are 0-7. this will roll over to 0 because it is only 3 bits
        dispPressed = false;
        setBrightness();
        writeBrightnessToEEPROM();
    }
}

void enableDisplay() {
    initBacklight();
    setBrightness();
}

void processIncomingCommand() {
    switch (rxData[0]) {
        case I2C_CMD_BRIGHT:
            if (rxData[1] < 8) {
                i2cdata.brightness = rxData[1];  // Store 0-7 to the status
            }
            break;
    }
}

void onRequest() {
    calculateCRC();
    Wire.write((const uint8_t*)&i2cdata, sizeof(i2cdata));
}

void onReceive(int numBytes) {
    if (numBytes == 4) {
        for(int i = 0; i < 4; i++) rxData[i] = Wire.read();
        pendingCommand = true;
    }
}

void readButtons() {
    // Read and invert in one operation
    uint16_t pressed = ~((PIND << 8) | PINB);
    uint16_t button_state = 0;
    
    for(uint8_t i = 0; i < 16; i++) {
        // Check if currently pressed
        if(pressed & (1 << i)) {
            button_counters[i] = BTN_DEBOUNCE_DURATION;
            button_state |= (1 << i);  // Set bit immediately if pressed
        }
        // Not pressed but still in duration window
        else if(button_counters[i] > 0) {
            button_counters[i]--;
            button_state |= (1 << i);
        }
    }
    i2cdata.buttons=button_state;
}

void setup() {
  initGPIOs();
  readEEPROM();
  enableDisplay();
  generateCRCTable();
  Wire.begin(I2C_ADDR);
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);
}

void loop() {
  if (pendingCommand) { // always immediately process incoming i2c command
    processIncomingCommand();
    pendingCommand = false;
  }

  unsigned long currentTime = millis();
  if (UPDATE_INTERVAL_REACHED) {    // Check if it's time to scan inputs
    lastUpdateTime = currentTime;
    readJoysticks();
    readButtons();
    checkDisplayButton();
  }
}
