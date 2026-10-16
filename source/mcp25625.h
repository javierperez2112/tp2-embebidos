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

#define CAN_MAX_DLC         8

#define MCP_TX_QUEUE_SIZE   16      // frames esperando un buffer de TX del MCP
#define MCP_RX_QUEUE_SIZE   32      // frames recibidos esperando a la app


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
    bool     loopback;      // los frames enviados vuelven a RX sin salir al bus
} mcp_cfg_t;


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Resetea y configura el MCP25625 (bit timing, filtros, modo). Usa SPI
 *        bloqueante, asi que se puede llamar con las interrupciones apagadas.
 * @return false si el controlador no respondio o el bitrate no es posible
 */
bool mcpInit (const mcp_cfg_t *config);

/**
 * @brief Encola un frame para enviar. No bloquea.
 * @return false si la cola de TX esta llena (o el driver no se inicializo)
 */
bool mcpSend (const can_frame_t *frame);

/**
 * @brief Saca el frame recibido mas viejo
 * @return false si no habia frames recibidos
 */
bool mcpReceive (can_frame_t *frame);

/**
 * @brief Frames recibidos que se perdieron por tener la cola de RX llena
 */
uint32_t mcpGetRxDropped (void);


/*******************************************************************************
 ******************************************************************************/

#endif // _MCP25625_H_
