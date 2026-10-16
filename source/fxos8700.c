/***************************************************************************//**
  @file     fxos8700.c
  @brief    Acelerometro/magnetometro FXOS8700CQ por I2C: rolido, cabeceo y
            orientacion
  @author   grupo 2
 ******************************************************************************/

#include "fxos8700.h"
#include "i2c.h"
#include "board.h"

// TODO: registros (WHO_AM_I 0x0D, CTRL_REG1 0x2A, M_CTRL_REG1 0x5B, ...),
//       lectura en rafaga desde STATUS (0x00), calculo de angulos

bool fxosInit (void)
{
    return false;
}

void fxosRequestRead (void)
{
}

bool fxosGetAngles (fxos_angles_t *angles)
{
    (void)angles;
    return false;
}
