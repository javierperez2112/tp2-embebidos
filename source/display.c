/***************************************************************************//**
  @file     display.c
  @brief    Driver for a 4 digit 7-segment multiplexed display wired behind a
            shift register chain.
  @author   Franco Dorfman
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "display.h"
#include "shift_register.h"
#include "pisr.h"

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define SEGMENTS 7 //this is to avoid a magic number
#define LED_MAX 3

// Layout of the frame pushed into the shift register chain, in the order the
// bits leave the MCU (first pushed travels furthest down the chain):
//   status LEDs (2) | digit select (2) | segments a..g (7) | decimal point (1)
#define STATUS_BITS 2
#define SELECT_BITS 2

// Multiplexing is driven entirely by pisr, which calls displayMuxPoll() on
// every tick (1 tick = 1 ms, see pisr.h). Each digit owns a slot of
// DISPLAY_SLOT_TICKS consecutive ticks, so the whole 4-digit frame is scanned
// every MAX_CHARS*DISPLAY_SLOT_TICKS = 12 ms (83 Hz), above the flicker
// threshold.
//
// Brightness rides on that same subdivision: the digit is latched and its
// outputs enabled on the first tick of the slot, and disabled again once
// `brightness` ticks have gone by. That gives DISPLAY_SLOT_TICKS levels
// (1/3, 2/3 and 3/3 of the slot) and modulates at 333 Hz, well clear of
// anything the eye resolves. Widening the slot would buy more levels at the
// cost of refresh rate, so the two numbers are deliberately the same.
#define DISPLAY_SLOT_TICKS  3
#define DISPLAY_POLL_TICKS  1

// Mascaras de segmentos, bit 0 = a, bit 1 = b ... bit 6 = g, un 1 enciende.
// El display de la placa es de anodo comun, pero cada segmento cuelga de un
// BC337 con el emisor a masa (ver el esquematico de la Display-Encoder Board),
// asi que la linea en alto hace conducir al transistor y prende el segmento.
// Es decir: la convencion de bits es la de catodo comun, y es la correcta.
static const char segment_map[128] = {
    // Numbers 0 - 9
    ['0'] = 0x3F, // a,b,c,d,e,f
    ['1'] = 0x06, // b,c
    ['2'] = 0x5B, // a,b,g,e,d
    ['3'] = 0x4F, // a,b,c,d,g
    ['4'] = 0x66, // b,c,f,g
    ['5'] = 0x6D, // a,c,d,f,g
    ['6'] = 0x7D, // a,c,d,e,f,g
    ['7'] = 0x07, // a,b,c
    ['8'] = 0x7F, // a,b,c,d,e,f,g
    ['9'] = 0x6F, // a,b,c,d,f,g

    // Letters A - F (Hexadecimal & Capitals)
    ['A'] = 0x77, ['a'] = 0x5F,
    ['B'] = 0x7C, ['b'] = 0x7C, // Lowercase b looks like 6 or b
    ['C'] = 0x39, ['c'] = 0x58,
    ['D'] = 0x5E, ['d'] = 0x5E,
    ['E'] = 0x79, ['e'] = 0x7B,
    ['F'] = 0x71, ['f'] = 0x71,
	['G'] = 0x7D, ['g'] = 0x7D, // same as 6

    // Additional readable letters (approximate on 7-segment)
    ['H'] = 0x76, ['h'] = 0x74,
    ['I'] = 0x04, ['i'] = 0x04,
    ['L'] = 0x38, ['l'] = 0x06,
    ['N'] = 0x54, ['n'] = 0x54,
    ['O'] = 0x3F, ['o'] = 0x5C,
    ['P'] = 0x73, ['p'] = 0x73,
    ['R'] = 0x50, ['r'] = 0x50,
    ['S'] = 0x6D, ['s'] = 0x6D,
    ['U'] = 0x3E, ['u'] = 0x1C,
    ['Y'] = 0x6E, ['y'] = 0x6E,

    // Common Symbols
    ['-'] = 0x40, // Middle segment (g)
    ['_'] = 0x08, // Bottom segment (d)
    ['='] = 0x48, // g and d segments
    [' '] = 0x00  // Blank / Off
};

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 * VARIABLE WITH FILE SCOPE
 ******************************************************************************/

static char dispChars[MAX_CHARS];
static bool charRefresh[MAX_CHARS];
static uint8_t ledStatus;
static bool decPoint[MAX_CHARS];
//read by the mux running in the pisr ISR, written from the app
static volatile uint8_t brightness = DISPLAY_BRIGHT_MAX;

/*******************************************************************************
  * FUNCTION PROTOTYPES WITH FILE SCOPE
  ******************************************************************************/

static void copyStrings(char* si, char* so);
static void pushDigitFrame(uint8_t i);
static void displayMuxPoll(void);

/*******************************************************************************
 * FUNCTION DEFINITIONS WITH GLOBAL SCOPE
 ******************************************************************************/

/*
 * @brief Initialize the display and the shift register chain it hangs from.
 */
void displayInit(void){
	int i;
	for(i=0;i<MAX_CHARS;i++){
		charRefresh[i]=true;
		decPoint[i]=false;
		dispChars[i]=' ';
	}
	ledStatus=0;
	brightness=DISPLAY_BRIGHT_MAX;
	SR_Init();
	SR_OutputEnable(true);
	//the app never drives the mux: pisr calls displayMuxPoll() on its own,
	//cycling through the digits, forever, no callback needed on the app side.
	pisrRegister(displayMuxPoll, DISPLAY_POLL_TICKS);
}

/*
 * @brief Set the display as the given string.
 * Periodic refresh must be enabled to see changes
 * @params string String to write
 */
void displayWriteString(char string[MAX_CHARS]){
	copyStrings(string,dispChars);
}

/*
 * @brief Copy string currently written on display into string.
 * @params Pointer to string to copy the display's contents.
 */
void displayGetString(char string[MAX_CHARS]){
	copyStrings(dispChars, string);
}

/*
 * @brief Disable character from being turned on at refresh.
 * This function can be used to blink a character using timers.
 * @params n Character to disable. 0 is rightmost and MAX_CHARS-1 is leftmost.
 */
void displayDisableChar(uint8_t n){
	charRefresh[n] = false;
}

/*
 * @brief Enable character so it lights up during refresh.
 * This function can be used to blink a character using timers.
 * @params n Character to enable. 0 is rightmost and MAX_CHARS-1 is leftmost.
 */
void displayEnableChar(uint8_t n){
	charRefresh[n] = true;
}

/*
 * @brief Set decimal point at nth character
 * @params n Character to set decimal point. p Set to HIGH or LOW.
 */
void setDecimalPoint(uint8_t n, bool p){
	if(n<MAX_CHARS){
		decPoint[n] = p;
	}
}

/*
 * @brief Set the display brightness (see display.h).
 */
void displaySetBrightness(uint8_t level){
	if(level < DISPLAY_BRIGHT_MIN){
		level = DISPLAY_BRIGHT_MIN;
	}
	if(level > DISPLAY_BRIGHT_MAX){
		level = DISPLAY_BRIGHT_MAX;
	}
	brightness = level;
}

/*
 * @brief Get the brightness level currently applied.
 */
uint8_t displayGetBrightness(void){
	return brightness;
}

/*
 * @brief Set status LEDs on board. They share the shift register frame with
 * the display, so they are latched out by the multiplexing scan.
 */
void setLEDStatus(uint8_t status){
	if(status<=LED_MAX){
		ledStatus = status;
	}
}

/*******************************************************************************
 * FUNCTION DEFINITIONS WITH FILE SCOPE
 ******************************************************************************/

/*
 * @brief pisr callback (registered from displayInit): runs every tick and owns
 * both the multiplexing and the dimming. This is the only place that cycles
 * digits; the app never drives the scan.
 *
 * Each digit owns DISPLAY_SLOT_TICKS consecutive ticks:
 *   tick 0                  latch the digit and enable the outputs
 *   tick == brightness      disable the outputs for the rest of the slot
 * so the digit is lit for `brightness` of the DISPLAY_SLOT_TICKS ticks.
 */
static void displayMuxPoll(void){
	static uint8_t muxIndex = 0;
	static uint8_t slotTick = 0;

	if(slotTick == 0){
		if(charRefresh[muxIndex]){
			pushDigitFrame(muxIndex);
			SR_OutputEnable(true);
		}
		else{
			//character switched off on purpose (blinking): the outputs stay
			//disabled for the whole slot. Simply not latching anything would
			//leave the PREVIOUS digit lit through this slot, which reads as
			//that neighbour shining at double brightness.
			SR_OutputEnable(false);
		}
	}
	else if(slotTick >= brightness){
		SR_OutputEnable(false);
	}

	if(++slotTick >= DISPLAY_SLOT_TICKS){
		slotTick = 0;
		muxIndex = (uint8_t)((muxIndex + 1) % MAX_CHARS);
	}
}

/*
 * @brief Shift and latch the frame that lights up character i.
 */
static void pushDigitFrame(uint8_t i){
	uint8_t nbit;
	//first the status leds
	SR_PushBits(ledStatus, STATUS_BITS);
	//then the display select
	SR_PushBits(i, SELECT_BITS);
	//get character code from map
	char code = segment_map[dispChars[i] & 0x7F];//mask to prevent reading out of bounds
	//read code bit by bit, segment a first
	for(nbit=0;nbit<SEGMENTS;nbit++){
		SR_PushBit((code&(1U<<nbit))!=0);
	}
	//send decimal point
	SR_PushBit(decPoint[i]);
	//send to latch
	SR_Latch();
}

static void copyStrings(char* si, char* so){
	int i;
	for(i=0;i<MAX_CHARS;i++){
		so[i] = si[i];
	}
}

/*******************************************************************************
 ******************************************************************************/
