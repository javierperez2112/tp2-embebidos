/***************************************************************************//**
  @file     board.h
  @brief    Board management
  @author   Nicolás Magliola
 ******************************************************************************/

#ifndef _BOARD_H_
#define _BOARD_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "gpio.h"


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

/***** BOARD defines **********************************************************/

// On Board User LEDs
#define PIN_LED_RED     PORTNUM2PIN(PB,22) // PTB22
#define PIN_LED_GREEN   PORTNUM2PIN(PE,26) // PTE26
#define PIN_LED_BLUE    PORTNUM2PIN(PB,21) // PTB21

#define LED_ACTIVE      LOW


// On Board User Switches
#define PIN_SW2        PORTNUM2PIN(PC,6) // PTC6
#define PIN_SW3        PORTNUM2PIN(PA,4) // PTA4

#define SW_ACTIVE       // ???
#define SW_INPUT_TYPE   // ???

// Pin de testeo

#define TEST_PIN      PORTNUM2PIN(PC, 10)

// Pinout del lector
#define MAGCARD_DATA_PIN       PORTNUM2PIN(PB, 10)      // DATA   - Azul  - LSB sale primero
#define MAGCARD_CLOCK_PIN      PORTNUM2PIN(PB, 3)      // CLOCK  - Verde - dato cambia en flanco positivo
#define MAGCARD_ENABLE_PIN     PORTNUM2PIN(PB, 2)      // ENABLE - Amarillo - bajo mientras la tarjeta desliza

// Pinout del encoder
#define PIN_ENC_A 		   	PORTNUM2PIN(PD, 1)
#define PIN_ENC_B 		   	PORTNUM2PIN(PD, 0)
#define PIN_ENC_C 		   	PORTNUM2PIN(PD, 2)

// Pinout del display 7 segmentos (detras de la cadena de shift registers)
#define PIN_DISP_SRQ       PORTNUM2PIN(PC, 4)  // SER    - dato serie hacia el primer 74HC595
#define PIN_DISP_RCLK      PORTNUM2PIN(PC, 3)  // RCLK   - latch clock
#define PIN_DISP_SRCLR     PORTNUM2PIN(PC, 2)  // SRCLR# - clear del shift stage, activo bajo
#define PIN_DISP_SRCLK     PORTNUM2PIN(PC, 16) // SRCLK  - shift clock
#define PIN_DISP_OE        PORTNUM2PIN(PC, 17) // OE#    - output enable, activo bajo

/*******************************************************************************
 ******************************************************************************/

#endif // _BOARD_H_
