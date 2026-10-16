/***************************************************************************//**
  @file     shift_register.h
  @brief    Generic driver for a chain of serial-in / parallel-out shift
            registers (74HC595 style) driven by bit-banged GPIO.
            Knows nothing about what is wired to its outputs.
  @author   Franco Dorfman
 ******************************************************************************/

#ifndef SHIFT_REGISTER_H_
#define SHIFT_REGISTER_H_


/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define SR_MAX_BITS     32      // limit of a single SR_PushBits() call

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 * VARIABLE PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/*
 * @brief Configure the shift register pins, clear the chain and latch it.
 * Outputs are left disabled (OE high); call SR_OutputEnable(true) when the
 * first meaningful frame has been latched.
 */
void SR_Init(void);

/*
 * @brief Shift a single bit into the chain. Does not latch.
 * @params bit Value to shift in.
 */
void SR_PushBit(bool bit);

/*
 * @brief Shift the nbits least significant bits of data into the chain,
 * most significant bit first. Does not latch.
 * @params data Bits to shift in. nbits How many, up to SR_MAX_BITS.
 */
void SR_PushBits(uint32_t data, uint8_t nbits);

/*
 * @brief Move the contents of the shift stage to the output stage (RCLK pulse).
 */
void SR_Latch(void);

/*
 * @brief Shift nbits of data (MSB first) and latch them in one call.
 * @params data Bits to shift in. nbits How many, up to SR_MAX_BITS.
 */
void SR_Write(uint32_t data, uint8_t nbits);

/*
 * @brief Clear the shift stage (SRCLR pulse) and latch the zeros out.
 */
void SR_Clear(void);

/*
 * @brief Enable or disable the parallel outputs (OE pin).
 * @params enable true drives the outputs, false tri-states them.
 */
void SR_OutputEnable(bool enable);


/*******************************************************************************
 ******************************************************************************/


#endif /* SHIFT_REGISTER_H_ */
