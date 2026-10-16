#include "encoder.h"
#include "board.h"
#include "gpio.h"
#include "pisr.h"
#include <stddef.h>

#define BUTTON_POLL_PERIOD           1U   // 1 tick de pisr (1ms) -> 1kHz, solo para el boton
#define ENCODER_CLICK_DEBOUNCE_POLLS 5U

// Indexed by (prevAB << 2 | currAB). Invalid jumps (bounce/noise) map to 0
static const int8_t transitionTable[16] = {
	 0, -1,  1,  0,
	 1,  0,  0, -1,
	-1,  0,  0,  1,
	 0,  1, -1,  0
};

static volatile int32_t position = 0;
static volatile bool    clickFlag = false;

static volatile uint8_t lastAB = 0;
static bool    buttonPressed = false;
static uint8_t buttonStableCount = 0;

static void encoderEdgeISR (void);
static void buttonPoll (void);

bool Encoder_Init (void)
{
	gpioMode(PIN_ENC_A, INPUT_PULLUP);
	gpioMode(PIN_ENC_B, INPUT_PULLUP);
	gpioMode(PIN_ENC_C, INPUT_PULLUP);

	lastAB = (gpioRead(PIN_ENC_A) << 1) | gpioRead(PIN_ENC_B);
	buttonPressed = false;
	buttonStableCount = 0;
	clickFlag = false;

	// A/B se decodifican por interrupcion, no por poleo: a 1kHz de poleo se
	// puede perder un estado intermedio justo al invertir el sentido de
	// giro (el rebote mecanico en ese instante es mas rapido que 1ms), y esa
	// cuenta perdida es la que se nota como "un paso salteado" al arrancar a
	// girar para el otro lado. Por interrupcion se atiende cada flanco apenas
	// ocurre, asi que nunca se salta un estado de la secuencia de Gray.
	GPIO_Interrupt(PIN_ENC_A, IRQ_RISEFALL, encoderEdgeISR);
	GPIO_Interrupt(PIN_ENC_B, IRQ_RISEFALL, encoderEdgeISR);

	return pisrRegister(buttonPoll, BUTTON_POLL_PERIOD);
}

int32_t Encoder_GetCount (void)
{
	return position;
}

void Encoder_SetCount (int32_t count)
{
	position = count;
}

bool Encoder_GetClick (void)
{
	bool clicked = clickFlag;
	clickFlag = false;
	return clicked;
}

static void encoderEdgeISR (void)
{
	uint8_t ab = (gpioRead(PIN_ENC_A) << 1) | gpioRead(PIN_ENC_B);
	position += transitionTable[(lastAB << 2) | ab];
	lastAB = ab;
}

static void buttonPoll (void)
{
	bool pinLow = !gpioRead(PIN_ENC_C);

	if (pinLow == buttonPressed) {
		buttonStableCount = 0;
	} else if (++buttonStableCount >= ENCODER_CLICK_DEBOUNCE_POLLS) {
		buttonPressed = pinLow;
		buttonStableCount = 0;
		if (buttonPressed) {
			clickFlag = true; // flanco descendente ya debounceado
		}
	}
}
