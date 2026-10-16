/***************************************************************************//**
  @file     i2c.h
  @brief    I2C master driver, no bloqueante (transacciones por interrupciones)
  @author   grupo 2
 ******************************************************************************/

#ifndef _I2C_H_
#define _I2C_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef enum {
    I2C_IDLE,
    I2C_BUSY,
    I2C_DONE,
    I2C_ERROR,      // NACK o arbitraje perdido
} i2c_status_t;

typedef void (*i2c_callback_t) (void);


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Inicializa el I2C como master y sus pines (segun board.h)
 * @param id I2C a usar
 * @param baudrate frecuencia de SCL deseada
 */
void i2cInit (uint8_t id, uint32_t baudrate);

/**
 * @brief Arranca una transaccion: escribe wrLen bytes y, si rdLen > 0, hace
 *        repeated start y lee rdLen bytes. No bloquea.
 * @param callback se llama (desde la ISR) al terminar, puede ser NULL
 * @return false si habia otra transaccion en curso
 */
bool i2cStartTransaction (uint8_t id, uint8_t address,
                          const uint8_t *wrBuf, uint8_t wrLen,
                          uint8_t *rdBuf, uint8_t rdLen,
                          i2c_callback_t callback);

/**
 * @brief Estado de la ultima transaccion
 */
i2c_status_t i2cGetStatus (uint8_t id);


/*******************************************************************************
 ******************************************************************************/

#endif // _I2C_H_
