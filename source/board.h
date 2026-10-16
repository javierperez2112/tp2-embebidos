/***************************************************************************//**
  @file     board.h
  @brief    Board management (FRDM-K64F + PlacaCAN por SPI)
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

// Numero de la funcion de un pin en PORTx_PCRn[MUX]
#define PORT_MUX_GPIO       1

/***** BOARD defines **********************************************************/

// On Board User LEDs
#define PIN_LED_RED     PORTNUM2PIN(PB,22) // PTB22
#define PIN_LED_GREEN   PORTNUM2PIN(PE,26) // PTE26
#define PIN_LED_BLUE    PORTNUM2PIN(PB,21) // PTB21

#define LED_ACTIVE      LOW


// On Board User Switches
#define PIN_SW2         PORTNUM2PIN(PC,6) // PTC6 (compartido con FXOS8700 INT1)
#define PIN_SW3         PORTNUM2PIN(PA,4) // PTA4

#define SW_ACTIVE       LOW             // pull-up externo en la placa, aprieta a GND
#define SW_INPUT_TYPE   INPUT


// Pin de testeo: en alto mientras corre una ISR, para medir uso de CPU
#define TEST_PIN        PORTNUM2PIN(PC,10) // PTC10


/***** UART: PC (puerto COM virtual del OpenSDA) ******************************/

#define PC_UART_ID          0                   // UART0
#define PIN_PC_UART_RX      PORTNUM2PIN(PB,16)  // PTB16 - UART0_RX
#define PIN_PC_UART_TX      PORTNUM2PIN(PB,17)  // PTB17 - UART0_TX
#define PC_UART_MUX         3                   // ALT3


/***** I2C: acelerometro/magnetometro FXOS8700CQ (en la placa) ****************/

#define FXOS_I2C_ID         0                   // I2C0
#define PIN_FXOS_SCL        PORTNUM2PIN(PE,24)  // PTE24 - I2C0_SCL
#define PIN_FXOS_SDA        PORTNUM2PIN(PE,25)  // PTE25 - I2C0_SDA
#define FXOS_I2C_MUX        5                   // ALT5, open drain (pull-ups en la placa)

#define FXOS_I2C_ADDR       0x1D                // SA1 = 0, SA0 = 1 en la FRDM-K64F

#define PIN_FXOS_INT1       PORTNUM2PIN(PC,6)   // PTC6  - compartido con SW2
#define PIN_FXOS_INT2       PORTNUM2PIN(PC,13)  // PTC13


/***** SPI: PlacaCAN (controller + transceiver MCP25625) **********************/

/*
 * Conexionado minimo FRDM-K64F <-> PlacaCAN (header Arduino de la FRDM):
 *
 *   FRDM            PlacaCAN
 *   D10 PTD0  -->   J4.7  CS    (activo bajo, pull-up en la PlacaCAN)
 *   D13 PTD1  -->   J4.4  SCK
 *   D11 PTD2  -->   J4.5  MOSI  (SI del MCP25625)
 *   D12 PTD3  <--   J4.6  MISO  (SO del MCP25625)
 *   D2  PTB9  <--   J4.3  INT   (activo bajo)
 *   D4  PTB23 -->   J4.8  RESET (activo bajo, pull-up en la PlacaCAN)
 *   GND       ---   J4.1 / J4.11
 *
 * Jumpers de la PlacaCAN:
 *   J7, J8: puestos (se usan controller y transceiver juntos)
 *   J9:     VREGIO-VIO (logica a 3,3V desde el regulador de la PlacaCAN)
 *   J5:     5V-VCAN, con VCC_CAN = 5V desde el bus
 *   J1, J2: solo si la placa esta en un extremo del bus (terminador 120 ohm)
 *
 * No unir VIO ni 5V con la FRDM si la PlacaCAN ya los genera: cortocircuito.
 */

#define CAN_SPI_ID          0                   // SPI0
#define PIN_CAN_SPI_PCS0    PORTNUM2PIN(PD,0)   // PTD0 - SPI0_PCS0
#define PIN_CAN_SPI_SCK     PORTNUM2PIN(PD,1)   // PTD1 - SPI0_SCK
#define PIN_CAN_SPI_SOUT    PORTNUM2PIN(PD,2)   // PTD2 - SPI0_SOUT (MOSI)
#define PIN_CAN_SPI_SIN     PORTNUM2PIN(PD,3)   // PTD3 - SPI0_SIN  (MISO)
#define CAN_SPI_MUX         2                   // ALT2
#define CAN_SPI_PCS         0                   // chip select por hardware: PCS0

// MCP25625: latchea en flanco ascendente, cambia en descendente -> modo 0,0 (o 1,1)
#define CAN_SPI_CPOL        0
#define CAN_SPI_CPHA        0
#define CAN_SPI_MAX_HZ      10000000U           // fCLK_MAX del MCP25625

#define PIN_CAN_INT         PORTNUM2PIN(PB,9)   // PTB9  - INT del MCP25625, activo bajo
#define PIN_CAN_RESET       PORTNUM2PIN(PB,23)  // PTB23 - RESET del MCP25625, activo bajo

#define CAN_INT_ACTIVE      LOW
#define CAN_RESET_ACTIVE    LOW

#define CAN_OSC_HZ          16000000U           // cristal X1 de la PlacaCAN

/*******************************************************************************
 ******************************************************************************/

#endif // _BOARD_H_
