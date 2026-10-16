/***************************************************************************//**
  @file     spi.h
  @brief    SPI (DSPI) master driver, no bloqueante
  @author   grupo 2
 ******************************************************************************/

#ifndef _SPI_H_
#define _SPI_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef struct {
    uint32_t baudrate;      // frecuencia de SCK deseada
    uint8_t  cpol;          // 0: SCK en reposo bajo
    uint8_t  cpha;          // 0: captura en el primer flanco
    uint8_t  pcs;           // chip select por hardware a usar
    bool     lsbFirst;
} spi_cfg_t;

typedef void (*spi_callback_t) (void);


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Inicializa el SPI como master y sus pines (segun board.h)
 */
void spiInit (uint8_t id, const spi_cfg_t *config);

/**
 * @brief Transfiere len bytes con el CS activo durante todo el frame. Lo
 *        recibido se guarda en rxBuf (puede ser NULL). No bloquea.
 * @param callback se llama (desde la ISR) al terminar, puede ser NULL
 * @return false si habia otra transferencia en curso
 */
bool spiTransfer (uint8_t id, const uint8_t *txBuf, uint8_t *rxBuf,
                  uint8_t len, spi_callback_t callback);

/**
 * @brief Si hay una transferencia en curso
 */
bool spiIsBusy (uint8_t id);


/*******************************************************************************
 ******************************************************************************/

#endif // _SPI_H_
