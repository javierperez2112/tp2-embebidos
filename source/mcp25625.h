/***************************************************************************//**
  @file     mcp25625.h
  @brief    Controlador CAN externo MCP25625 (PlacaCAN) por SPI
  @author   grupo 2
 ******************************************************************************/

#ifndef _MCP25625_H_
#define _MCP25625_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define CAN_MAX_DLC     8


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

// Frame CAN 2.0A (ID estandar de 11 bits)
typedef struct {
    uint16_t id;
    uint8_t  len;
    uint8_t  data[CAN_MAX_DLC];
} can_frame_t;

typedef struct {
    uint32_t bitrate;       // bits/s
    uint16_t filterId;      // acepta los ID con (id & filterMask) == filterId
    uint16_t filterMask;
} mcp_cfg_t;


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Resetea y configura el MCP25625 (bit timing, filtros, modo normal)
 * @return false si el controlador no respondio
 */
bool mcpInit (const mcp_cfg_t *config);

/**
 * @brief Carga un frame en un buffer de TX libre y pide su envio. No bloquea.
 * @return false si no habia buffer de TX libre
 */
bool mcpSend (const can_frame_t *frame);

/**
 * @brief Saca el frame recibido mas viejo
 * @return false si no habia frames recibidos
 */
bool mcpReceive (can_frame_t *frame);


/*******************************************************************************
 ******************************************************************************/

#endif // _MCP25625_H_
