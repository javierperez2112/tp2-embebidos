/***************************************************************************//**
  @file     gpio.h
  @brief    Simple GPIO Pin services, similar to Arduino
  @author   Nicolás Magliola
 ******************************************************************************/

#ifndef _GPIO_H_
#define _GPIO_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

// Ports
enum { PA, PB, PC, PD, PE };

// Convert port and number into pin ID
// Ex: PTB5  -> PORTNUM2PIN(PB,5)  -> 0x25
//     PTC22 -> PORTNUM2PIN(PC,22) -> 0x56
#define PORTNUM2PIN(p,n)    (((p)<<5) + (n))
#define PIN2PORT(p)         (((p)>>5) & 0x07)
#define PIN2NUM(p)          ((p) & 0x1F)


// Modes
#ifndef INPUT
#define INPUT               0
#define OUTPUT              1
#define INPUT_PULLUP        2
#define INPUT_PULLDOWN      3
#endif // INPUT


// Digital values
#ifndef LOW
#define LOW     0
#define HIGH    1
#endif // LOW


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef uint8_t pin_t;


/*******************************************************************************
 * VARIABLE PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

/**
 * @brief Configures the specified pin to behave either as an input or an output
 * @param pin the pin whose mode you wish to set (according PORTNUM2PIN)
 * @param mode INPUT, OUTPUT, INPUT_PULLUP or INPUT_PULLDOWN.
 */
void gpioMode (pin_t pin, uint8_t mode);

/**
 * @brief Write a HIGH or a LOW value to a digital pin
 * @param pin the pin to write (according PORTNUM2PIN)
 * @param val Desired value (HIGH or LOW)
 */
void gpioWrite (pin_t pin, bool value);

/**
 * @brief Toggle the value of a digital pin (HIGH<->LOW)
 * @param pin the pin to toggle (according PORTNUM2PIN)
 */
void gpioToggle (pin_t pin);

/**
 * @brief Reads the value from a specified digital pin, either HIGH or LOW.
 * @param pin the pin to read (according PORTNUM2PIN)
 * @return HIGH or LOW
 */
bool gpioRead (pin_t pin);


enum IRQ_MODES {IRQ_LOW=8, IRQ_RISING, IRQ_FALLING, IRQ_RISEFALL, IRQ_HIGH};

bool GPIO_Interrupt (pin_t pin, int mode, void (*funcallback)(void));


/*******************************************************************************
 * PIN DE TESTEO (TP)
 ******************************************************************************/

/*
 * Pin de testeo pedido por la consigna: queda en alto mientras se ejecuta una
 * interrupcion, para medir en el osciloscopio cuanto tiempo se emplea en las
 * ISR y que porcentaje del total representa. El ciclo de trabajo de esta senal
 * es ese porcentaje, se lee directo de la pantalla del osciloscopio.
 *
 * Vive aca y no adentro de un driver porque no le pertenece a ninguno: lo
 * levantan todas las ISR del programa, las de puerto y la periodica. Se
 * configura solo, desde el primer driver que arme una interrupcion, asi que
 * queda listo antes de que pueda dispararse la primera y no depende del orden
 * en que la aplicacion inicialice los drivers.
 */

/*
 * @brief Configura el pin de testeo como salida, en bajo. Es idempotente: la
 * llaman los drivers al habilitar sus interrupciones y solo la primera hace
 * algo.
 */
void tpInit (void);

/*
 * @brief Marca el comienzo de una ISR: pone el TP en alto.
 * Lleva la cuenta de las ISR anidadas, asi que si alguna vez se le diera mas
 * prioridad a una interrupcion que a otra el pin seguiria midiendo bien, sin
 * bajar hasta que termine la ultima. Cada enter tiene que tener su exit.
 */
void tpIsrEnter (void);

/*
 * @brief Marca el fin de una ISR: baja el TP si no queda ninguna anidada.
 */
void tpIsrExit (void);


/*******************************************************************************
 ******************************************************************************/

#endif // _GPIO_H_
