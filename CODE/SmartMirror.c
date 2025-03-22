/** ###################################################################
**     Filename  : SmartMirror.c
**     Project   : SmartMirror
**     Processor : MC9S12XS128CAL
**     Version   : Driver 01.14
**     Compiler  : CodeWarrior HCS12X C Compiler
**     Date/Time : 2025-03-22, 18:03
**     Abstract  :
**         Main module.
**         This module contains user's application code.
**     Settings  :
**     Contents  :
**         No public methods
**
** ###################################################################*/
/* MODULE SmartMirror */

/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
#include "SPI1.h"
#include "PWM8.h"
#include "CAN1.h"
#include "GPIO1.h"
/* Include shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */

// Define CH452 registers and commands
#define CH452_CMD_DECODE_MODE    0x00
#define CH452_CMD_BRIGHTNESS     0x01
#define CH452_CMD_WRITE_DISPLAY  0x02
#define CH452_CMD_READ_KEY       0x03
#define CH452_CMD_SCAN_KEY       0x04
#define CH452_CMD_SLEEP          0x05

// Define CAN message IDs
#define CAN_ID_SEND              0x200
#define CAN_ID_RECEIVE           0x100

// Function declarations
bool isPJ0Pressed(void);
bool sendCanMessage(word id, byte *data, byte length);
bool waitForCanMessage(word id, dword *value1, dword *value2);
void displayOnLED(dword value1, dword value2);
void initCH452(void);
void CH452_SendCommand(byte cmd, byte data);

void main(void)
{
  /* Write your local variable definition here */
  bool keyPressed = FALSE;
  dword receivedValue1 = 0;
  dword receivedValue2 = 0;

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  
  // Initialize CH452 LED driver
  initCH452();
  
  // Main program loop
  for(;;) {
    // Check if PJ0 is pressed
    if (isPJ0Pressed() && !keyPressed) {
      keyPressed = TRUE;
      
      // Send blank CAN message with ID 0x200
      sendCanMessage(CAN_ID_SEND, NULL, 0);
      
      // Wait for CAN message with ID 0x100
      if (waitForCanMessage(CAN_ID_RECEIVE, &receivedValue1, &receivedValue2)) {
        // Display values on LED
        displayOnLED(receivedValue1, receivedValue2);
      }
    }
    else if (!isPJ0Pressed()) {
      keyPressed = FALSE;
    }
  }

  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

// Function to check if PJ0 is pressed
bool isPJ0Pressed(void) {
  // Read PJ0 (bit 0 of Port J) - Assuming active low (pressed = 0)
  return (PTJ & 0x01) == 0;
}

// Function to send a CAN message
bool sendCanMessage(word id, byte *data, byte length) {
  byte i;
  byte txBuffer = 0;
  
  // Wait for a transmit buffer to be available
  while ((CAN0TFLG & 0x07) == 0) {}
  
  // Select the first available buffer
  txBuffer = CAN0TFLG & 0x07;
  CAN0TBSEL = txBuffer;
  
  // Set the ID (standard 11-bit ID)
  CAN0TXIDR0 = (byte)(id >> 3);
  CAN0TXIDR1 = (byte)((id << 5) & 0xE0);
  
  // Set data length
  CAN0TXDLR = length & 0x0F;
  
  // Copy data if any
  if (data != NULL && length > 0) {
    for (i = 0; i < length; i++) {
      *(&CAN0TXDSR0 + i) = data[i];
    }
  }
  
  // Transmit the message
  CAN0TFLG = txBuffer;
  
  return TRUE;
}

// Function to wait for a CAN message
bool waitForCanMessage(word id, dword *value1, dword *value2) {
  word timeoutCounter = 0;
  word receivedID;
  
  // Wait for a message or timeout
  while (!(CAN0RFLG & 0x01)) {
    timeoutCounter++;
    if (timeoutCounter > 10000) {
      return FALSE; // Timeout
    }
  }
  
  // Check if the ID matches
  receivedID = (word)(CAN0RXIDR0 << 3) | (word)((CAN0RXIDR1 >> 5) & 0x07);
  if (receivedID != id) {
    // Clear the flag and return
    CAN0RFLG = 0x01;
    return FALSE;
  }
  
  // Extract the two int32 values
  *value1 = ((dword)CAN0RXDSR0 << 24) | ((dword)CAN0RXDSR1 << 16) | 
            ((dword)CAN0RXDSR2 << 8) | CAN0RXDSR3;
  *value2 = ((dword)CAN0RXDSR4 << 24) | ((dword)CAN0RXDSR5 << 16) | 
            ((dword)CAN0RXDSR6 << 8) | CAN0RXDSR7;
  
  // Clear the flag
  CAN0RFLG = 0x01;
  
  return TRUE;
}

// Function to initialize CH452 LED driver
void initCH452(void) {
  // Set decode mode to HEX
  CH452_SendCommand(CH452_CMD_DECODE_MODE, 0x00);
  
  // Set brightness to maximum
  CH452_SendCommand(CH452_CMD_BRIGHTNESS, 0x0F);
}

// Function to send a command to CH452
void CH452_SendCommand(byte cmd, byte data) {
  // Using SPI1 to send command and data
  byte txBuffer[2];
  
  txBuffer[0] = cmd;
  txBuffer[1] = data;
  
  // Send command and data via SPI
  SPI1_SendBlock(txBuffer, 2, NULL);
}

// Function to display values on LED
void displayOnLED(dword value1, dword value2) {
  byte i;
  byte digit;
  
  // Display value1 on first 4 digits (display 0)
  for (i = 0; i < 4; i++) {
    // Extract each digit (working with 4 bits at a time for hex display)
    digit = (byte)((value1 >> (i * 4)) & 0x0F);
    CH452_SendCommand(CH452_CMD_WRITE_DISPLAY | i, digit);
  }
  
  // Display value2 on next 4 digits (display 1)
  for (i = 0; i < 4; i++) {
    digit = (byte)((value2 >> (i * 4)) & 0x0F);
    CH452_SendCommand(CH452_CMD_WRITE_DISPLAY | (i + 4), digit);
  }
}

/* END SmartMirror */
/*
** ###################################################################
**
**     This file was created by Processor Expert 3.02 [04.44]
**     for the Freescale HCS12X series of microcontrollers.
**
** ###################################################################
*/
