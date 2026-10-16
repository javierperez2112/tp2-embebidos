/***************************************************************************//**
  @file     fxos8700.h
  @brief    Acelerometro/magnetometro FXOS8700CQ por I2C: rolido, cabeceo y
            orientacion
  @author   grupo 2
 ******************************************************************************/

#ifndef _FXOS8700_H_
#define _FXOS8700_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

// Angulos en grados, de -179 a 180
typedef struct {
    int16_t roll;       // rolido
    int16_t pitch;      // cabeceo
    int16_t yaw;        // orientacion: 0 Norte, 90 Este, 180 Sur, -90 Oeste
} fxos_angles_t;


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Configura el sensor (modo hibrido acel + magnetometro)
 * @return false si el sensor no respondio (WHO_AM_I != 0xC7)
 */
bool fxosInit (void);

/**
 * @brief Pide una nueva lectura por I2C. No bloquea.
 */
void fxosRequestRead (void);

/**
 * @brief Devuelve los ultimos angulos calculados
 * @return true si hubo datos nuevos desde la ultima llamada
 */
bool fxosGetAngles (fxos_angles_t *angles);


/*******************************************************************************
 ******************************************************************************/

#endif // _FXOS8700_H_
