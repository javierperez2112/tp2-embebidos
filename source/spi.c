/***************************************************************************//**
  @file     spi.c
  @brief    SPI (DSPI) master driver, no bloqueante
  @author   grupo 2
 ******************************************************************************/

#include "spi.h"
#include "board.h"
#include "MK64F12.h"
#include "hardware.h"

// TODO: CTAR (baud rate, CPOL/CPHA, frame de 8 bits), PUSHR con CONT para
//       mantener el CS en todo el frame, ISR de SPI0 (TFFF/RFDF) o DMA

void spiInit (uint8_t id, const spi_cfg_t *config)
{
    (void)id;
    (void)config;
}

bool spiTransfer (uint8_t id, const uint8_t *txBuf, uint8_t *rxBuf,
                  uint8_t len, spi_callback_t callback)
{
    (void)id;
    (void)txBuf;
    (void)rxBuf;
    (void)len;
    (void)callback;
    return false;
}

bool spiIsBusy (uint8_t id)
{
    (void)id;
    return false;
}
