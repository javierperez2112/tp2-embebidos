/***************************************************************************//**
  @file     station.c
  @brief    Protocolo CAN entre estaciones: frames de angulos y de LEDs
  @author   grupo 2
 ******************************************************************************/

#include "station.h"

void stationEncodeAngle (can_frame_t *frame, char angleId, int16_t angle)
{
    (void)frame;
    (void)angleId;
    (void)angle;
}

void stationEncodeLeds (can_frame_t *frame, uint8_t group,
                        bool red, bool green, bool blue)
{
    (void)frame;
    (void)group;
    (void)red;
    (void)green;
    (void)blue;
}

void stationDecode (const can_frame_t *frame, station_msg_t *msg)
{
    (void)frame;
    msg->type = STATION_MSG_INVALID;
}
