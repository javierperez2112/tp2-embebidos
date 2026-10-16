/***************************************************************************//**
  @file     uart.h
  @brief    UART driver, no bloqueante (buffers circulares + interrupciones)
  @author   grupo 2
 ******************************************************************************/

#ifndef _UART_H_
#define _UART_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define UART_CANT_IDS   5
#define UART_BUFF_SIZE  64


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef struct {
    uint32_t baudrate;
} uart_cfg_t;


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Inicializa la UART y sus pines (segun board.h)
 * @param id UART a usar
 * @param config configuracion deseada
 */
void uartInit (uint8_t id, uart_cfg_t config);

/**
 * @brief Cuantos bytes recibidos esperan ser leidos
 */
uint8_t uartGetRxMsgLength (uint8_t id);

/**
 * @brief Copia hasta cant bytes recibidos a msg
 * @return cantidad de bytes copiados
 */
uint8_t uartReadMsg (uint8_t id, char *msg, uint8_t cant);

/**
 * @brief Encola cant bytes para transmitir. No bloquea.
 * @return cantidad de bytes encolados (menos que cant si el buffer se lleno)
 */
uint8_t uartWriteMsg (uint8_t id, const char *msg, uint8_t cant);

/**
 * @brief Si ya se termino de transmitir todo lo encolado
 */
bool uartIsTxMsgComplete (uint8_t id);


/*******************************************************************************
 ******************************************************************************/

#endif // _UART_H_
