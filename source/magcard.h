/***************************************************************************//**
  @file     magcard.h
  @brief    Driver para lector
  @author   tomi
 ******************************************************************************/

#ifndef _MAGCARD_H_
#define _MAGCARD_H_

/*******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

/*******************************************************************************
 * CONSTANTES
 ******************************************************************************/

// Tamaños maximos de buffers internos
#define MAGCARD_MAX_CHARS      40
#define MAGCARD_MAX_PAN_LEN    20   // PAN (hasta 19 digitos) + '\0'
#define MAGCARD_MAX_DATA_LEN   24   // Fecha venc. + service code + datos discrecionales + '\0'

// Timeout en ms sin actividad de CLOCK durante una lectura en curso,
// antes de abortarla (tarjeta retirada a mitad de camino, lectura
// trabada, etc). Se controla por polling contra millis(), no hace
// falta una interrupcion periodica para esto.
#define MAGCARD_TIMEOUT_MS  500


typedef enum {
    MAGCARD_IDLE = 0,          // sin lectura en curso ni resultado pendiente
    MAGCARD_READING,           // swipe en curso, todavia no hay resultado
    MAGCARD_OK,                 // ultima lectura valida (PAN/datos disponibles)
    MAGCARD_ERROR_PARITY,       // paridad impar de algun caracter invalida
    MAGCARD_ERROR_LRC,          // LRC de la trama invalido
    MAGCARD_ERROR_FORMAT,       // ENABLE volvio a HIGH sin haber visto SS...ES
    MAGCARD_ERROR_OVERFLOW      // la trama supero MAGCARD_MAX_CHARS
} magcard_status_t;

/*******************************************************************************
 * Prototipos
 ******************************************************************************/

/**
 * @brief Inicializa el driver del lector
 */
void MagCard_Init (void);

/**
 * @brief Devuelve el estado de la ultima lectura. La app la va poleando
 *        desde el loop principal; ahi mismo se resuelve el timeout si
 *        paso demasiado tiempo sin actividad de CLOCK.
 */
magcard_status_t MagCard_GetStatus (void);

/**
 * @brief Copia el resultado de la ultima lectura exitosa (Track 2) del lector.
 */
bool MagCard_GetTrack2 (char* pan, uint8_t panLen, char* data, uint8_t dataLen);

/**
 * @brief Limpia el resultado de la ultima lectura
 */
void MagCard_Clear (void);

/*******************************************************************************
 ******************************************************************************/

#endif // _MAGCARD_H_
