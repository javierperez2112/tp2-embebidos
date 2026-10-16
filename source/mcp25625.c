/***************************************************************************//**
  @file     mcp25625.c
  @brief    Controlador CAN externo MCP25625 (PlacaCAN) por SPI
  @author   grupo 2
 ******************************************************************************/

#include "mcp25625.h"
#include "spi.h"
#include "gpio.h"
#include "board.h"

// TODO: instrucciones SPI (RESET 0xC0, READ 0x03, WRITE 0x02, BIT MODIFY 0x05,
//       READ STATUS 0xA0, RTS 0x8x), CNF1..3 para el bitrate con CAN_OSC_HZ,
//       mascaras/filtros RXMn/RXFn, interrupcion por PIN_CAN_INT

bool mcpInit (const mcp_cfg_t *config)
{
    (void)config;
    return false;
}

bool mcpSend (const can_frame_t *frame)
{
    (void)frame;
    return false;
}

bool mcpReceive (can_frame_t *frame)
{
    (void)frame;
    return false;
}
