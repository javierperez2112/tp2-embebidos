/***************************************************************************//**
  @file     App.c
  @brief    Estacion de inclinacion (TP2 - Comunicacion Serie).

            Por ahora: prueba de los drivers de SPI y del MCP25625. Se elige la
            prueba con APP_TEST:

            TEST_SPI_LOOP     Solo el SPI. Puentear MOSI con MISO (PTD2 - PTD3,
                              D11 - D12) sin la PlacaCAN. Cada 100ms manda 16
                              bytes y compara lo recibido con lo enviado.

            TEST_CAN_LOOPBACK SPI + MCP25625 en modo loopback: los frames no
                              salen al bus, vuelven por RX. No hace falta otra
                              placa ni terminador. Cada 100ms manda un frame y
                              espera recibirlo igual.

            TEST_CAN_BUS      MCP25625 en modo normal, contra otra estacion.
                              Cada 500ms manda 'R' + un contador en ASCII con el
                              ID propio, y titila el LED azul con cada frame
                              recibido. Solo, sin otro nodo que de ACK, el frame
                              se reintenta para siempre y la cola de TX se llena.

            LEDs:
              rojo fijo al arrancar   fallo la inicializacion (MCP no responde)
              verde titilando         cada prueba que sale bien
              rojo                    hubo algun error (queda prendido)
              azul                    titila con cada frame recibido (CAN_BUS)

            Los contadores test* son globales para mirarlos con el debugger.
            El pin de testeo (TEST_PIN) sube durante las ISR de SPI y de INT.
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "board.h"
#include "gpio.h"
#include "pisr.h"
#include "spi.h"
#include "mcp25625.h"
#include "station.h"
#include <stddef.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define TEST_SPI_LOOP       0
#define TEST_CAN_LOOPBACK   1
#define TEST_CAN_BUS        2

#ifndef APP_TEST
#define APP_TEST            TEST_CAN_LOOPBACK
#endif

#define SPI_TEST_LEN        16
#define SPI_TEST_PERIOD_MS  100

#define CAN_LOOP_PERIOD_MS  100
#define CAN_LOOP_TIMEOUT_MS 50
#define CAN_BUS_PERIOD_MS   500


/*******************************************************************************
 * VARIABLES WITH GLOBAL SCOPE (para el debugger)
 ******************************************************************************/

volatile uint32_t testOk     = 0;
volatile uint32_t testErrors = 0;
volatile uint32_t testRx     = 0;
volatile bool     testInitOk = false;


/*******************************************************************************
 * VARIABLES WITH LOCAL SCOPE
 ******************************************************************************/

static uint32_t lastMs;


/*******************************************************************************
 * LOCAL FUNCTION DEFINITIONS
 ******************************************************************************/

static void ledWrite (pin_t pin, bool on)
{
    gpioWrite(pin, on ? LED_ACTIVE : !LED_ACTIVE);
}

static void ledsInit (void)
{
    gpioMode(PIN_LED_RED, OUTPUT);
    gpioMode(PIN_LED_GREEN, OUTPUT);
    gpioMode(PIN_LED_BLUE, OUTPUT);
    ledWrite(PIN_LED_RED, false);
    ledWrite(PIN_LED_GREEN, false);
    ledWrite(PIN_LED_BLUE, false);
}

static void reportOk (void)
{
    testOk++;
    gpioToggle(PIN_LED_GREEN);
}

static void reportError (void)
{
    testErrors++;
    ledWrite(PIN_LED_RED, true);
}


#if APP_TEST == TEST_SPI_LOOP

static uint8_t          spiTxBuf[SPI_TEST_LEN];
static uint8_t          spiRxBuf[SPI_TEST_LEN];
static volatile bool    spiDone = false;
static uint8_t          seed = 0;

static void onSpiTestDone (void)
{
    spiDone = true;
}

static bool testInit (void)
{
    spi_cfg_t cfg = {
        .baudrate = 2000000U,
        .cpol     = CAN_SPI_CPOL,
        .cpha     = CAN_SPI_CPHA,
        .pcs      = CAN_SPI_PCS,
        .lsbFirst = false,
    };
    if (!spiInit(CAN_SPI_ID, &cfg))
    {
        return false;
    }

    // Una vuelta bloqueante: si MOSI-MISO no esta puenteado se lee 0xFF
    uint8_t tx[2] = { 0x5A, 0xA5 };
    uint8_t rx[2] = { 0 };
    spiTransferBlocking(CAN_SPI_ID, tx, rx, 2);
    return (rx[0] == tx[0]) && (rx[1] == tx[1]);
}

static void testRun (void)
{
    if (spiDone)
    {
        spiDone = false;

        bool ok = true;
        for (uint8_t i = 0; i < SPI_TEST_LEN; i++)
        {
            ok = ok && (spiRxBuf[i] == spiTxBuf[i]);
        }
        ok ? reportOk() : reportError();
    }

    if (!spiIsBusy(CAN_SPI_ID) && ((millis() - lastMs) >= SPI_TEST_PERIOD_MS))
    {
        lastMs = millis();

        for (uint8_t i = 0; i < SPI_TEST_LEN; i++)
        {
            spiTxBuf[i] = (uint8_t)(seed + 37U * i);
            spiRxBuf[i] = 0;
        }
        seed++;

        spiTransfer(CAN_SPI_ID, spiTxBuf, spiRxBuf, SPI_TEST_LEN, onSpiTestDone);
    }
}

#elif APP_TEST == TEST_CAN_LOOPBACK

static can_frame_t  sent;
static bool         waiting = false;
static uint32_t     sentMs;
static uint8_t      counter = 0;

static bool testInit (void)
{
    mcp_cfg_t cfg = {
        .bitrate    = CAN_BITRATE,
        .filterId   = STATION_BASE_ID,
        .filterMask = STATION_FILTER_MASK,
        .loopback   = true,
    };
    return mcpInit(&cfg);
}

static bool sameFrame (const can_frame_t *a, const can_frame_t *b)
{
    if ((a->id != b->id) || (a->len != b->len))
    {
        return false;
    }
    for (uint8_t i = 0; i < a->len; i++)
    {
        if (a->data[i] != b->data[i])
        {
            return false;
        }
    }
    return true;
}

static void testRun (void)
{
    can_frame_t rx;

    while (mcpReceive(&rx))
    {
        testRx++;
        if (waiting && sameFrame(&rx, &sent))
        {
            waiting = false;
            reportOk();
        }
        else
        {
            reportError();
        }
    }

    if (waiting && ((millis() - sentMs) >= CAN_LOOP_TIMEOUT_MS))
    {
        waiting = false;
        reportError();
    }

    if (!waiting && ((millis() - lastMs) >= CAN_LOOP_PERIOD_MS))
    {
        lastMs = millis();

        // Largo y contenido cambiantes, para probar todos los DLC
        sent.id  = STATION_OWN_ID;
        sent.len = (uint8_t)(counter % (CAN_MAX_DLC + 1));
        for (uint8_t i = 0; i < sent.len; i++)
        {
            sent.data[i] = (uint8_t)(counter + i);
        }
        counter++;

        if (mcpSend(&sent))
        {
            waiting = true;
            sentMs = millis();
        }
        else
        {
            reportError();
        }
    }
}

#elif APP_TEST == TEST_CAN_BUS

static uint8_t counter = 0;

static bool testInit (void)
{
    mcp_cfg_t cfg = {
        .bitrate    = CAN_BITRATE,
        .filterId   = STATION_BASE_ID,
        .filterMask = STATION_FILTER_MASK,
        .loopback   = false,
    };
    return mcpInit(&cfg);
}

static void testRun (void)
{
    can_frame_t rx;

    while (mcpReceive(&rx))
    {
        testRx++;
        gpioToggle(PIN_LED_BLUE);
    }

    if ((millis() - lastMs) >= CAN_BUS_PERIOD_MS)
    {
        lastMs = millis();

        can_frame_t f = { .id = STATION_OWN_ID, .len = 4 };
        f.data[0] = ANGLE_ROLL;
        f.data[1] = (uint8_t)('0' + (counter / 100) % 10);
        f.data[2] = (uint8_t)('0' + (counter / 10) % 10);
        f.data[3] = (uint8_t)('0' + counter % 10);
        counter++;

        mcpSend(&f) ? reportOk() : reportError();
    }
}

#else
#error "APP_TEST invalido"
#endif


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

/* Función que se llama 1 vez, al comienzo del programa */
void App_Init (void)
{
    pisrInit();
    ledsInit();

    testInitOk = testInit();
    if (!testInitOk)
    {
        ledWrite(PIN_LED_RED, true);
    }

    lastMs = millis();
}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run (void)
{
    if (testInitOk)
    {
        testRun();
    }
}
