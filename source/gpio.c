/*
 * gpio.c
 *
 *  Created on: 10 ago. 2026
 *      Author: tomi
 */

#include "MK64F12.h"
#include "gpio.h"
#include "board.h"
#include "hardware.h"
#include <stddef.h>

typedef uint8_t pin_t;

GPIO_Type* GPIOx[5] = GPIO_BASE_PTRS;
PORT_Type* PORTx[5] = PORT_BASE_PTRS;

static void gpioClockEnable(uint32_t port_n){
	SIM->SCGC5 |= (1UL << (SIM_SCGC5_PORTA_SHIFT + port_n));
}

/**
 * @brief Configures the specified pin to behave either as an input or an output
 * @param pin the pin whose mode you wish to set (according PORTNUM2PIN)
 * @param mode INPUT, OUTPUT, INPUT_PULLUP or INPUT_PULLDOWN.
 */
void gpioMode (pin_t pin, uint8_t mode){
	uint32_t port_n = ((pin)>>5) & 0x07;
	uint32_t pin_n = (pin) & 0x1F;

	GPIO_Type* GPIO = GPIOx[port_n];
	PORT_Type* PORT = PORTx[port_n];

	gpioClockEnable(port_n);

	PORT->PCR[pin_n] = (PORT->PCR[pin_n] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);

	if (mode == 1){
		GPIO->PDDR |= (1UL << pin_n);
	}
	else{
		GPIO->PDDR &= ~(1UL << pin_n);
		if(mode == 2){
			PORT->PCR[pin_n] = (PORT->PCR[pin_n] & ~(PORT_PCR_PE_MASK | PORT_PCR_PS_MASK)) // agarro el registro y cleareo los bits que quiero editar
			                  | PORT_PCR_PE(1) | PORT_PCR_PS(1);				// hago un OR del registro con los set que quiero hacer
		}
		else if (mode==3){
			PORT->PCR[pin_n] = (PORT->PCR[pin_n] & ~(PORT_PCR_PE_MASK | PORT_PCR_PS_MASK))
			                  | PORT_PCR_PE(1) | PORT_PCR_PS(0);
		}
		else{
			PORT->PCR[pin_n] = (PORT->PCR[pin_n] & ~PORT_PCR_PE_MASK);
		}
	}
}

/**
 * @brief Write a HIGH or a LOW value to a digital pin
 * @param pin the pin to write (according PORTNUM2PIN)
 * @param val Desired value (HIGH or LOW)
 */
void gpioWrite (pin_t pin, bool value){
	uint32_t port_n = ((pin)>>5) & 0x07;
	uint32_t pin_n = (pin) & 0x1F;

	GPIO_Type* GPIO = GPIOx[port_n];

	if (value){
		GPIO->PSOR = 1UL << pin_n;
	}
	else{
		GPIO->PCOR = 1UL << pin_n;
	}
}

/**
 * @brief Toggle the value of a digital pin (HIGH<->LOW)
 * @param pin the pin to toggle (according PORTNUM2PIN)
 */
void gpioToggle (pin_t pin){
	uint32_t port_n = ((pin)>>5) & 0x07;
	uint32_t pin_n = (pin) & 0x1F;

	GPIO_Type* GPIO = GPIOx[port_n];

	GPIO->PTOR = 1UL << pin_n;
}

/**
 * @brief Reads the value from a specified digital pin, either HIGH or LOW.
 * @param pin the pin to read (according PORTNUM2PIN)
 * @return HIGH or LOW
 */
bool gpioRead (pin_t pin){
	uint32_t port_n = ((pin)>>5) & 0x07;
	uint32_t pin_n = (pin) & 0x1F;

	GPIO_Type* GPIO = GPIOx[port_n];

	return (GPIO->PDIR >> pin_n) & 1;
}

/*******************************************************************************
 * PIN DE TESTEO (TP)
 ******************************************************************************/

static bool              tpReady = false;
static volatile uint8_t  tpDepth = 0;

void tpInit (void)
{
	if (!tpReady)
	{
		tpReady = true;
		tpDepth = 0;
		gpioMode(TEST_PIN, OUTPUT);
		gpioWrite(TEST_PIN, LOW);
	}
}

void tpIsrEnter (void)
{
	// El contador se toca con las interrupciones enmascaradas: si una ISR mas
	// prioritaria cayera justo en el medio del incremento, la cuenta se
	// perderia y el pin quedaria mintiendo. Son unos pocos ciclos, y transcurren
	// con el TP ya en alto, asi que quedan adentro de lo que el propio TP mide
	// en lugar de escaparsele.
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	if (tpDepth++ == 0)
	{
		gpioWrite(TEST_PIN, HIGH);
	}

	__set_PRIMASK(primask);
}

void tpIsrExit (void)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	if ((tpDepth > 0) && (--tpDepth == 0))
	{
		gpioWrite(TEST_PIN, LOW);
	}

	__set_PRIMASK(primask);
}

/*******************************************************************************
 * INTERRUPCIONES DE PUERTO
 ******************************************************************************/

void (*callbacks_GPIO[5][32])(void) = {NULL};

bool GPIO_Interrupt (pin_t pin, int mode, void (*funcallback)(void))
{
	uint32_t port_n = ((pin)>>5) & 0x07;
	uint32_t pin_n = (pin) & 0x1F;

	PORT_Type* PORT = PORTx[port_n];

	tpInit();	// antes de armar la interrupcion, no despues

	PORT->PCR[pin_n] = (PORT->PCR[pin_n] & ~(PORT_PCR_IRQC_MASK)) | PORT_PCR_IRQC(mode);

	callbacks_GPIO[port_n][pin_n] = funcallback;

	NVIC_SetPriority(PORTA_IRQn + port_n, 1);
	NVIC_EnableIRQ(PORTA_IRQn + port_n);

	return 1;
}

__ISR__ PORTA_IRQHandler (void)
{
	tpIsrEnter();			// TP: arranca el tiempo en ISR
	NVIC_ClearPendingIRQ(PORTA_IRQn);
	for (int pin_n=0 ; pin_n < 32 ; pin_n++){
		if(PORTA->PCR[pin_n] & PORT_PCR_ISF_MASK){
			PORTA->PCR[pin_n] |= PORT_PCR_ISF_MASK;
			if (callbacks_GPIO[0][pin_n] != NULL){
				callbacks_GPIO[0][pin_n]();
			}
		}
	}
	tpIsrExit();			// TP: fin del tiempo en ISR
}

__ISR__ PORTB_IRQHandler (void)
{
	tpIsrEnter();			// TP: arranca el tiempo en ISR
	NVIC_ClearPendingIRQ(PORTB_IRQn);
	for (int pin_n = 0; pin_n < 32; pin_n++){
		if (PORTB->PCR[pin_n] & PORT_PCR_ISF_MASK){
			PORTB->PCR[pin_n] |= PORT_PCR_ISF_MASK;
			if (callbacks_GPIO[1][pin_n] != NULL){
				callbacks_GPIO[1][pin_n]();
			}
		}
	}
	tpIsrExit();			// TP: fin del tiempo en ISR
}

__ISR__ PORTC_IRQHandler (void)
{
	tpIsrEnter();			// TP: arranca el tiempo en ISR
	NVIC_ClearPendingIRQ(PORTC_IRQn);
	for (int pin_n = 0; pin_n < 32; pin_n++){
		if (PORTC->PCR[pin_n] & PORT_PCR_ISF_MASK){
			PORTC->PCR[pin_n] |= PORT_PCR_ISF_MASK;
			if (callbacks_GPIO[2][pin_n] != NULL){
				callbacks_GPIO[2][pin_n]();
			}
		}
	}
	tpIsrExit();			// TP: fin del tiempo en ISR
}

__ISR__ PORTD_IRQHandler (void)
{
	tpIsrEnter();			// TP: arranca el tiempo en ISR
	NVIC_ClearPendingIRQ(PORTD_IRQn);
	for (int pin_n = 0; pin_n < 32; pin_n++){
		if (PORTD->PCR[pin_n] & PORT_PCR_ISF_MASK){
			PORTD->PCR[pin_n] |= PORT_PCR_ISF_MASK;
			if (callbacks_GPIO[3][pin_n] != NULL){
				callbacks_GPIO[3][pin_n]();
			}
		}
	}
	tpIsrExit();			// TP: fin del tiempo en ISR
}

__ISR__ PORTE_IRQHandler (void)
{
	tpIsrEnter();			// TP: arranca el tiempo en ISR
	NVIC_ClearPendingIRQ(PORTE_IRQn);
	for (int pin_n = 0; pin_n < 32; pin_n++){
		if (PORTE->PCR[pin_n] & PORT_PCR_ISF_MASK){
			PORTE->PCR[pin_n] |= PORT_PCR_ISF_MASK;
			if (callbacks_GPIO[4][pin_n] != NULL){
				callbacks_GPIO[4][pin_n]();
			}
		}
	}
	tpIsrExit();			// TP: fin del tiempo en ISR
}
