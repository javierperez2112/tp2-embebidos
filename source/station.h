/***************************************************************************//**
  @file     station.h
  @brief    Protocolo CAN entre estaciones: frames de angulos y de LEDs
  @author   grupo 2
 ******************************************************************************/

#ifndef _STATION_H_
#define _STATION_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "mcp25625.h"


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define GROUP_NUMBER        2

#define CAN_BITRATE         125000U
#define STATION_BASE_ID     0x100
#define STATION_CANT        8                       // ID 0x100 a 0x107
#define STATION_OWN_ID      (STATION_BASE_ID + GROUP_NUMBER)
#define STATION_FILTER_MASK 0x7F8                   // acepta 0x100..0x107

#define ANGLE_ROLL          'R'
#define ANGLE_PITCH         'C'
#define ANGLE_YAW           'O'


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef enum {
    STATION_MSG_ANGLE,
    STATION_MSG_LEDS,
    STATION_MSG_INVALID,
} station_msg_type_t;

typedef struct {
    station_msg_type_t type;
    uint8_t  station;       // 0..7, sale del ID del frame
    char     angleId;       // ANGLE_ROLL, ANGLE_PITCH o ANGLE_YAW
    int16_t  angle;
    uint8_t  ledGroup;      // grupo destino del mensaje de LEDs
    bool     red, green, blue;
} station_msg_t;


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Arma el frame 'R-34', 'C0', 'O180', ... con el ID propio
 */
void stationEncodeAngle (can_frame_t *frame, char angleId, int16_t angle);

/**
 * @brief Arma el frame de 1 byte 1JKL 0RGB con el ID propio
 */
void stationEncodeLeds (can_frame_t *frame, uint8_t group,
                        bool red, bool green, bool blue);

/**
 * @brief Interpreta y valida un frame recibido
 */
void stationDecode (const can_frame_t *frame, station_msg_t *msg);


/*******************************************************************************
 ******************************************************************************/

#endif // _STATION_H_
