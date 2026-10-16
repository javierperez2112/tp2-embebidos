/***************************************************************************//**
  @file     App.c
  @brief    Estacion de inclinacion (TP2 - Comunicacion Serie).

            I2C  -> fxos8700.h : rolido, cabeceo y orientacion propios
            SPI  -> mcp25625.h : bus CAN con las otras estaciones (station.h)
            UART -> uart.h     : envio de todas las estaciones a la PC y
                                 recepcion de los comandos de LEDs
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "board.h"
#include "gpio.h"
#include "pisr.h"
#include "uart.h"
#include "fxos8700.h"
#include "mcp25625.h"
#include "station.h"


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define PC_UART_BAUDRATE    115200U


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

/* Función que se llama 1 vez, al comienzo del programa */
void App_Init (void)
{
    pisrInit();

    gpioMode(PIN_LED_RED, OUTPUT);
    gpioMode(PIN_LED_GREEN, OUTPUT);
    gpioMode(PIN_LED_BLUE, OUTPUT);
    gpioWrite(PIN_LED_RED, !LED_ACTIVE);
    gpioWrite(PIN_LED_GREEN, !LED_ACTIVE);
    gpioWrite(PIN_LED_BLUE, !LED_ACTIVE);

    uart_cfg_t uartCfg = { .baudrate = PC_UART_BAUDRATE };
    uartInit(PC_UART_ID, uartCfg);

    fxosInit();

    mcp_cfg_t canCfg = {
        .bitrate    = CAN_BITRATE,
        .filterId   = STATION_BASE_ID,
        .filterMask = STATION_FILTER_MASK,
    };
    mcpInit(&canCfg);
}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run (void)
{
    // TODO: leer angulos propios, enviarlos por CAN (20/s max si cambio >= 5
    //       grados, cada 2 s si no), recibir frames CAN, reenviar todo a la PC
    //       y atender los comandos de LEDs que lleguen por UART
}
