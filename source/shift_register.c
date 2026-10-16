/***************************************************************************//**
  @file     shift_register.c
  @brief    Generic driver for a chain of serial-in / parallel-out shift
            registers (74HC595 style) driven by bit-banged GPIO.
  @author   Franco Dorfman
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "shift_register.h"
#include "board.h"
#include "gpio.h"

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 * VARIABLE WITH FILE SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH FILE SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION DEFINITIONS WITH GLOBAL SCOPE
 ******************************************************************************/

void SR_Init(void){
	gpioMode(PIN_DISP_SRQ, OUTPUT);
	gpioMode(PIN_DISP_RCLK, OUTPUT);
	gpioMode(PIN_DISP_SRCLR, OUTPUT);
	gpioMode(PIN_DISP_SRCLK, OUTPUT);
	gpioMode(PIN_DISP_OE, OUTPUT);
	//keep the outputs off until someone latches a meaningful frame
	SR_OutputEnable(false);
	//idle level of the shift clock
	gpioWrite(PIN_DISP_SRCLK, LOW);
	//clear shift register and latch the zeros out
	SR_Clear();
}

void SR_PushBit(bool bit){
	//write to shift register
	gpioWrite(PIN_DISP_SRQ, bit);
	//move shift register
	gpioWrite(PIN_DISP_SRCLK, HIGH);
	gpioWrite(PIN_DISP_SRCLK, LOW);
}

void SR_PushBits(uint32_t data, uint8_t nbits){
	uint8_t n;
	if(nbits > SR_MAX_BITS){
		nbits = SR_MAX_BITS;
	}
	//most significant bit travels the furthest down the chain
	for(n = nbits; n > 0; n--){
		SR_PushBit((data & (1UL << (n-1))) != 0);
	}
}

void SR_Latch(void){
	gpioWrite(PIN_DISP_RCLK, HIGH);
	gpioWrite(PIN_DISP_RCLK, LOW);
}

void SR_Write(uint32_t data, uint8_t nbits){
	SR_PushBits(data, nbits);
	SR_Latch();
}

void SR_Clear(void){
	//pulso de clear (activo bajo) sobre el shift stage
	gpioWrite(PIN_DISP_SRCLR, LOW);
	gpioWrite(PIN_DISP_SRCLR, HIGH);
	//y lo latcheo a la salida para que los ceros se vean reflejados ahi tambien
	SR_Latch();
}

void SR_OutputEnable(bool enable){
	//OE is active low
	gpioWrite(PIN_DISP_OE, enable ? LOW : HIGH);
}

/*******************************************************************************
 * FUNCTION DEFINITIONS WITH FILE SCOPE
 ******************************************************************************/

/*******************************************************************************
 ******************************************************************************/
