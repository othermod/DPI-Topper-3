#!/bin/bash

# Set log file names
FUSE_LOG="fuse_flash.log"
FIRMWARE_LOG="firmware_flash.log"

# ANSI color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to run command and log output
run_and_log() {
    echo "Running command: $1"
    echo "Logging to: $2"
    # Run the command, tee output to log file and terminal
    eval "$1" 2>&1 | tee "$2"
    # Check the exit status of the command
    return ${PIPESTATUS[0]}
}

# Flash fuses
run_and_log "sudo avrdude -p m8 -c linuxgpio -U lfuse:w:0xE4:m -U hfuse:w:0xDC:m" "$FUSE_LOG"
FUSE_RESULT=$?

# Flash firmware
run_and_log "sudo avrdude -c linuxgpio -p atmega8 -U flash:w:topper.ino.arduino_standard.hex:i" "$FIRMWARE_LOG"
FIRMWARE_RESULT=$?

# Check results and display colored output
if [ $FUSE_RESULT -eq 0 ] && [ $FIRMWARE_RESULT -eq 0 ]; then
    echo -e "${GREEN}Both commands completed successfully.${NC}"
else
    echo -e "${RED}One or both commands failed. Check log files for details.${NC}"
fi

