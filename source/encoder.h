#ifndef ENCODER_H_
#define ENCODER_H_

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef enum {
	ENCODER_CW  =  1,	// Clockwise, i.e. increment
	ENCODER_CCW = -1	// Counter-clockwise, i.e. decrement
} encoder_dir_t;


/*******************************************************************************
 * FUNCTION PROTOTYPES WITH GLOBAL SCOPE
 ******************************************************************************/

// Arranca el polling de A/B/SW a 1kHz por su cuenta (via pisr), sin callbacks
bool Encoder_Init (void);

int32_t Encoder_GetCount (void);

void Encoder_SetCount (int32_t count);

// Devuelve true si hubo un click (flanco descendente, ya debounceado) desde
// la ultima consulta, y limpia el flag
bool Encoder_GetClick (void);


/*******************************************************************************
 ******************************************************************************/

#endif /* ENCODER_H_ */
