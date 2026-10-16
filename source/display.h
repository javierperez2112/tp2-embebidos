/***************************************************************************//**
  @file     display.h
  @brief    Driver for a 4 digit 7-segment multiplexed display wired behind a
            shift register chain (see shift_register.h). Owns the character
            buffer, the segment encoding and the multiplexing frame.
  @author   Franco Dorfman
 ******************************************************************************/

#ifndef DISPLAY_H_
#define DISPLAY_H_


/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define MAX_CHARS 4

// Niveles de intensidad admitidos por displaySetBrightness(). El techo lo pone
// la resolucion del pisr: el barrido reparte cada digito en DISPLAY_SLOT_TICKS
// ticks de 1 ms y apaga la salida cuando se cumplio la fraccion encendida, asi
// que hay tantos niveles como ticks dura un digito. Subir el numero de niveles
// obliga a alargar el slot, y eso baja el refresco.
#define DISPLAY_BRIGHT_MIN  1
#define DISPLAY_BRIGHT_MAX  3

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
 * @brief Initialize the display and the shift register chain it hangs from.
 * Also registers the multiplexing refresh as a pisr periodic task, so the
 * mux frame is pushed out on its own; the app never has to poll or hook a
 * timer callback to keep the display alive, it only ever calls
 * displayWriteString() (and friends) to say what should be on screen.
 */
void displayInit(void);

/*
 * @brief Set the display as the given string.
 * Periodic refresh must be enabled to see changes
 * @params string String to write
 */
void displayWriteString(char string[MAX_CHARS]);

/*
 * @brief Copy string currently written on display into string.
 * @params Pointer to string to copy the display's contents.
 */
void displayGetString(char string[MAX_CHARS]);

/*
 * @brief Disable character from being turned on at refresh.
 * This function can be used to blink a character using timers.
 * @params n Character to disable. 0 is rightmost and MAX_CHARS-1 is leftmost.
 */
void displayDisableChar(uint8_t n);

/*
 * @brief Enable character so it lights up during refresh.
 * This function can be used to blink a character using timers.
 * @params n Character to enable. 0 is rightmost and MAX_CHARS-1 is leftmost.
 */
void displayEnableChar(uint8_t n);

/*
 * @brief Set decimal point at nth character
 * @params n Character to set decimal point. p Set to HIGH or LOW.
 */
void setDecimalPoint(uint8_t n, bool p);

/*
 * @brief Set the display brightness.
 * Dimming is done inside the multiplexing scan: the digit is latched at the
 * start of its slot and the shift register outputs are disabled once the lit
 * fraction of the slot has elapsed, so the modulation runs at a few hundred
 * hertz and is not visible as flicker.
 * @params level DISPLAY_BRIGHT_MIN (dimmest) to DISPLAY_BRIGHT_MAX (full).
 * Values outside the range are clamped.
 */
void displaySetBrightness(uint8_t level);

/*
 * @brief Get the brightness level currently applied.
 */
uint8_t displayGetBrightness(void);

/*
 * @brief Set status LEDs on board.
 * They are not part of the display, but they share the shift register frame
 * with it, so they are latched out by the multiplexing scan.
 * Los 2 bits entran a U1B (MC74HC139) con la habilitacion atada a masa y
 * salidas activas en bajo: Y0 no esta conectada a ningun LED, y Y1, Y2 e Y3
 * manejan D1, D2 y D3. Por eso el 0 apaga todo y no enciende el primero.
 * @params status How the LEDS should be turned on.
 * 0 means none. 1 is the first, 2 is 2nd and 3 is the third.
 */
void setLEDStatus(uint8_t status);


/*******************************************************************************
 ******************************************************************************/


#endif /* DISPLAY_H_ */
