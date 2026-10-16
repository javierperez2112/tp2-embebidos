/*
 * magcard.c
 *
 *  Driver para lector de banda magnetica Track 2 (ISO/IEC 7811-2).
 *  100% por interrupciones sobre GPIO, sin polling ni codigo bloqueante.
 *
 *  Author: tomi
 */

#include "magcard.h"
#include "board.h"
#include "pisr.h"
#include <string.h>

/*******************************************************************************
 * CONFIGURACION
 ******************************************************************************/

// DATA invertido (activo bajo)
#define MAGCARD_DATA_INVERTED   1


/*******************************************************************************
 * PRIVATE TYPES
 ******************************************************************************/

typedef enum {
    ST_WAIT_ENABLE = 0,   // idle, esperando que empiece un swipe
    ST_WAIT_SS,            // sincronizando bit a bit hasta encontrar el SS
    ST_READ_DATA,          // ya alineado: acumulando caracteres hasta el ES
    ST_DONE                // ya vino el ES, el proximo caracter es el LRC
} magcard_state_t;

/*******************************************************************************
 * PRIVATE CONSTANTS
 ******************************************************************************/

static const char FS_CHAR = '=';
static const char ES_CHAR = '?';
#define SS_NIBBLE      0x0Bu
#define SS_PATTERN     0x0Bu

// paridad impar esperada para cada nibble (indexado por el valor 0..15)
static const uint8_t oddParityLUT[16] = {
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1
};

/*******************************************************************************
 * PRIVATE VARIABLES
 ******************************************************************************/

static volatile magcard_state_t state = ST_WAIT_ENABLE;
static volatile magcard_status_t status = MAGCARD_IDLE;

// Sincronizacion inicial: ventana deslizante de 5 bits para buscar el SS
static volatile uint8_t shiftReg;

// Armado del caracter actual (5 bits: 4 de datos + 1 de paridad)
static volatile uint8_t bitCount;     // bits recibidos del caracter actual (0..4)
static volatile uint8_t nibble;       // 4 bits de datos, LSB primero
static volatile uint8_t parityBit;

// Armado de la trama completa
static volatile char    frame[MAGCARD_MAX_CHARS];
static volatile uint8_t frameLen;
static volatile bool    fsFound;
static volatile uint8_t fsIndex;
static volatile uint8_t xorNibble;    // XOR acumulado de nibbles (para LRC)

// Resultado de la ultima lectura valida, ya separado en PAN / resto
static char panBuf[MAGCARD_MAX_PAN_LEN];
static char dataBuf[MAGCARD_MAX_DATA_LEN];

// Watchdog de lectura trabada: se controla por polling contra millis()
static volatile bool     timingActive = false;
static volatile uint32_t lastActivityMs = 0;

/*******************************************************************************
 * PRIVATE FUNCTION PROTOTYPES
 ******************************************************************************/

static void enableISR (void);
static void clockISR (void);
static void finishReading (magcard_status_t result);
static void parseFrame (void);

/*******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

void MagCard_Init (void)
{
    // pull-up: el lector idlea en HIGH (activo bajo). Sin este pull, con el
    // lector desconectado o en open-collector, las lineas flotan y generan
    // interrupciones espurias sin parar (tranca en la ISR, no entra al loop).
    gpioMode(MAGCARD_DATA_PIN, INPUT);
    gpioMode(MAGCARD_CLOCK_PIN, INPUT);
    gpioMode(MAGCARD_ENABLE_PIN, INPUT);

    state = ST_WAIT_ENABLE;
    status = MAGCARD_IDLE;
    frameLen = 0;
    shiftReg = 0;
    timingActive = false;

    // Dato valido justo despues del flanco positivo de CLOCK
    GPIO_Interrupt(MAGCARD_CLOCK_PIN, IRQ_FALLING, clockISR);

    // ENABLE: un unico pin con ambos flancos
    GPIO_Interrupt(MAGCARD_ENABLE_PIN, IRQ_RISEFALL, enableISR);
}

magcard_status_t MagCard_GetStatus (void)
{
    if (timingActive && (millis() - lastActivityMs) >= MAGCARD_TIMEOUT_MS)
    {
        finishReading(MAGCARD_ERROR_FORMAT);
    }

    return status;
}

bool MagCard_GetTrack2 (char* pan, uint8_t panLen, char* data, uint8_t dataLen)
{
    uint8_t i;

    if (status != MAGCARD_OK)
    {
        return false;
    }

    // El parseo
    parseFrame();

    if (pan != NULL && panLen > 0)
    {
        for (i = 0; (panBuf[i] != '\0') && (i < (uint8_t)(panLen - 1)); i++)
        {
            pan[i] = panBuf[i];
        }
        pan[i] = '\0';
    }

    if (data != NULL && dataLen > 0)
    {
        for (i = 0; (dataBuf[i] != '\0') && (i < (uint8_t)(dataLen - 1)); i++)
        {
            data[i] = dataBuf[i];
        }
        data[i] = '\0';
    }

    return true;
}

void MagCard_Clear (void)
{
    status = MAGCARD_IDLE;
}

/*******************************************************************************
 * PRIVATE FUNCTIONS
 ******************************************************************************/

static void finishReading (magcard_status_t result)
{
    status = result;
    state = ST_WAIT_ENABLE;
    timingActive = false;
}

/**
 * @note Se llama desde MagCard_GetTrack2(), en contexto de aplicacion
 *       (NO desde la ISR), para no hacer un recorrido de hasta
 *       40 caracteres dentro de una interrupcion.
 */
static void parseFrame (void)
{
    uint8_t i;
    uint8_t panLen = 0;
    uint8_t dataLen = 0;

    for (i = 0; i < frameLen && frame[i] != ES_CHAR; i++)
    {
        if (fsFound && i == fsIndex)
        {
            continue; // no copio el separador de campo en si
        }

        if (!fsFound || i < fsIndex)
        {
            if (panLen < (MAGCARD_MAX_PAN_LEN - 1))
            {
                panBuf[panLen++] = frame[i];
            }
        }
        else
        {
            if (dataLen < (MAGCARD_MAX_DATA_LEN - 1))
            {
                dataBuf[dataLen++] = frame[i];
            }
        }
    }

    panBuf[panLen] = '\0';
    dataBuf[dataLen] = '\0';
}

/**
 * @brief ISR de ENABLE (ambos flancos). LOW = arranca swipe, HIGH = termino.
 */
static void enableISR (void)
{
    if (gpioRead(MAGCARD_ENABLE_PIN) == LOW)
    {
        // Arranca una nueva lectura, reseteo la maquina de estados
        state = ST_WAIT_SS;
        status = MAGCARD_READING;

        frameLen = 0;
        fsFound = false;
        bitCount = 0;
        nibble = 0;
        xorNibble = 0;
        shiftReg = 0;

        lastActivityMs = millis();
        timingActive = true;
    }
    else
    {
        // Termino el swipe. Si todavia estabamos leyendo es una trama incompleta.
        if (status == MAGCARD_READING)
        {
            finishReading(MAGCARD_ERROR_FORMAT);
        }
    }
}

/**
 * @brief ISR de CLOCK (flanco de bajada).
 *
 *        Fase 1 (ST_WAIT_SS): la tarjeta arranca con una tira de ceros de
 *        sincronismo cuya longitud NO es multiplo de 5
 *
 *        Fase 2 (ST_READ_DATA / ST_DONE): ya alineados, se arman caracteres
 *        de a 5 bits (4 de datos LSB-first + 1 de paridad impar).
 */
static void clockISR (void)
{
    bool bit;
    char c;

    if (state == ST_WAIT_ENABLE)
    {
        return; // pulso de reloj fuera de un swipe activo: se ignora
    }

#if MAGCARD_DATA_INVERTED
    bit = !gpioRead(MAGCARD_DATA_PIN);
#else
    bit = gpioRead(MAGCARD_DATA_PIN);
#endif

    lastActivityMs = millis(); // hay actividad, reinicio el watchdog

    /*------------------- Fase 1: sincronizacion de bits -------------------*/
    if (state == ST_WAIT_SS)
    {
        // bit0 = el mas viejo, bit4 = el mas nuevo (el de paridad)
        shiftReg = (uint8_t)((shiftReg >> 1) | ((uint8_t)bit << 4));

        if (shiftReg == SS_PATTERN)
        {
            xorNibble = SS_NIBBLE;   // el SS entra en el calculo del LRC
            frameLen  = 0;
            fsFound   = false;
            bitCount  = 0;
            nibble    = 0;
            state     = ST_READ_DATA;
        }
        return;
    }

    /*------------------- Fase 2: armado de caracteres ---------------------*/
    if (bitCount < 4)
    {
        nibble |= ((uint8_t)bit << bitCount);
    }
    else
    {
        parityBit = (uint8_t)bit;
    }
    bitCount++;

    if (bitCount < 5)
    {
        return; // caracter todavia incompleto
    }
    bitCount = 0;

    // Chequeo de paridad IMPAR (nibble + bit de paridad) via LUT
    if (parityBit != oddParityLUT[nibble])
    {
        nibble = 0;
        finishReading(MAGCARD_ERROR_PARITY);
        return;
    }

    if (state == ST_READ_DATA)
    {
        xorNibble ^= nibble;

        if (frameLen >= MAGCARD_MAX_CHARS)
        {
            nibble = 0;
            finishReading(MAGCARD_ERROR_OVERFLOW);
            return;
        }

        c = (char)(nibble + 0x30);

        if (c == FS_CHAR && !fsFound)
        {
            fsFound = true;
            fsIndex = frameLen;
        }

        frame[frameLen++] = c;

        if (c == ES_CHAR)
        {
            state = ST_DONE; // el proximo caracter es el LRC
        }
    }
    else /* ST_DONE */
    {
        uint8_t expectedLrc = xorNibble & 0x0Fu;
        magcard_status_t result = (nibble == expectedLrc) ? MAGCARD_OK : MAGCARD_ERROR_LRC;
        nibble = 0;
        finishReading(result);
        return;
    }

    nibble = 0;
}
