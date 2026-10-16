/***************************************************************************//**
  @file     uart.c
  @brief    UART driver, no bloqueante (buffers circulares + interrupciones)
  @author   grupo 2
 ******************************************************************************/

#include "uart.h"
#include "board.h"
#include "MK64F12.h"
#include "hardware.h"

// TODO: buffers circulares de RX/TX, ISR de UART0 (RIE/TIE), baud rate (SBR/BRFA)

void uartInit (uint8_t id, uart_cfg_t config)
{
    (void)id;
    (void)config;
}

uint8_t uartGetRxMsgLength (uint8_t id)
{
    (void)id;
    return 0;
}

uint8_t uartReadMsg (uint8_t id, char *msg, uint8_t cant)
{
    (void)id;
    (void)msg;
    (void)cant;
    return 0;
}

uint8_t uartWriteMsg (uint8_t id, const char *msg, uint8_t cant)
{
    (void)id;
    (void)msg;
    (void)cant;
    return 0;
}

bool uartIsTxMsgComplete (uint8_t id)
{
    (void)id;
    return true;
}
