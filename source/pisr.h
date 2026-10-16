/***************************************************************************//**
  @file     pisr.h
  @brief    Periodic Interrupt (PISR) driver
  @author   Nicolás Magliola
 ******************************************************************************/

#ifndef _PISR_H_
#define _PISR_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdbool.h>
#include <stdint.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define SYSTICK_ISR_FREQUENCY_HZ     1000U

#define PISR_CANT        8


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef void (*pisr_callback_t) (void);


/*******************************************************************************
 * VARIABLE PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Register PISR callback
 * @param fun PISR function to be call periodically
 * @param period PISR period in ticks
 * @return Registration succeed
 */
bool pisrRegister (pisr_callback_t fun, unsigned int period);

/**
 * @brief Arranca el SysTick. Hay que llamarla una vez al inicio, aun si
 *        no se va a registrar ningun periodico, para que millis() ande.
 */
void pisrInit (void);

/**
 * @brief Milisegundos transcurridos desde el arranque
 */
uint32_t millis (void);


/*******************************************************************************
 ******************************************************************************/

#endif // _PISR_H_
