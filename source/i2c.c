/***************************************************************************//**
  @file     i2c.c
  @brief    I2C master driver, no bloqueante (transacciones por interrupciones)
  @author   grupo 2
 ******************************************************************************/

#include "i2c.h"
#include "board.h"
#include "MK64F12.h"
#include "hardware.h"

// TODO: maquina de estados en la ISR de I2C0 (start, address, write, repeated
//       start, read, stop), divisor de SCL (I2C_F)

void i2cInit (uint8_t id, uint32_t baudrate)
{
    (void)id;
    (void)baudrate;
}

bool i2cStartTransaction (uint8_t id, uint8_t address,
                          const uint8_t *wrBuf, uint8_t wrLen,
                          uint8_t *rdBuf, uint8_t rdLen,
                          i2c_callback_t callback)
{
    (void)id;
    (void)address;
    (void)wrBuf;
    (void)wrLen;
    (void)rdBuf;
    (void)rdLen;
    (void)callback;
    return false;
}

i2c_status_t i2cGetStatus (uint8_t id)
{
    (void)id;
    return I2C_IDLE;
}
