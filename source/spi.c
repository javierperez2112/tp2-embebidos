/***************************************************************************//**
  @file     spi.c
  @brief    SPI (DSPI) master driver, no bloqueante
  @author   grupo 2

            Frames de 8 bits con CTAR0. El CS es el PCS por hardware: se marca
            CONT en todos los bytes menos el ultimo, asi queda activo durante
            toda la transferencia y se suelta solo al final.

            La transferencia avanza por la interrupcion de RFDF (llego un byte
            al FIFO de RX): por cada byte recibido se carga el siguiente a
            transmitir. Asi nunca hay mas bytes en vuelo que la profundidad del
            FIFO y nunca se desborda el de RX. La CPU interviene una vez por
            byte, no espera nunca.
 ******************************************************************************/

#include "spi.h"
#include "board.h"
#include "MK64F12.h"
#include "hardware.h"
#include <stddef.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define BUS_CLOCK_HZ        (__CORE_CLOCK__ / 2U)   // OUTDIV2 = 1 en hw_Init
#define SPI_IRQ_PRIORITY    1

#define PIN_NONE            0xFF

#define SR_CLEAR_ALL        (SPI_SR_TCF_MASK | SPI_SR_EOQF_MASK | SPI_SR_TFUF_MASK | \
                             SPI_SR_TFFF_MASK | SPI_SR_RFOF_MASK | SPI_SR_RFDF_MASK)


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef struct {
    const uint8_t   *txBuf;
    uint8_t         *rxBuf;
    uint8_t          len;
    uint8_t          txCount;
    uint8_t          rxCount;
    uint8_t          pcs;
    spi_callback_t   callback;
    volatile bool    busy;
    bool             ready;
} spi_state_t;

typedef struct {
    pin_t   sck, sout, sin;
    uint8_t mux;
} spi_pins_t;


/*******************************************************************************
 * ROM CONST VARIABLES WITH FILE LEVEL SCOPE
 ******************************************************************************/

static SPI_Type * const spiPtrs[SPI_CANT_IDS] = SPI_BASE_PTRS;
static const IRQn_Type  spiIrqs[SPI_CANT_IDS] = SPI_IRQS;
static const uint8_t    fifoDepth[SPI_CANT_IDS] = { 4, 1, 1 };

// Solo SPI0 tiene pines en board.h. El PCS se configura aparte, segun config->pcs.
static const spi_pins_t spiPins[SPI_CANT_IDS] = {
    { PIN_CAN_SPI_SCK, PIN_CAN_SPI_SOUT, PIN_CAN_SPI_SIN, CAN_SPI_MUX },
    { PIN_NONE, PIN_NONE, PIN_NONE, 0 },
    { PIN_NONE, PIN_NONE, PIN_NONE, 0 },
};

static const uint8_t prescalers[4] = { 2, 3, 5, 7 };                // PBR
static const uint16_t scalers[16]  = { 2, 4, 6, 8, 16, 32, 64, 128, // BR
                                       256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };


/*******************************************************************************
 * VARIABLES WITH LOCAL SCOPE
 ******************************************************************************/

static spi_state_t spiState[SPI_CANT_IDS];


/*******************************************************************************
 * LOCAL FUNCTION DEFINITIONS
 ******************************************************************************/

static void pinMux (pin_t pin, uint8_t mux, bool pullup)
{
    static PORT_Type * const ports[] = PORT_BASE_PTRS;
    uint8_t port = PIN2PORT(pin);

    SIM->SCGC5 |= (SIM_SCGC5_PORTA_MASK << port);
    ports[port]->PCR[PIN2NUM(pin)] = PORT_PCR_MUX(mux) |
                                     (pullup ? (PORT_PCR_PE_MASK | PORT_PCR_PS_MASK) : 0);
}

static bool pcsPin (uint8_t id, uint8_t pcs, pin_t *pin)
{
    // Por ahora solo el PCS0 de SPI0, que es el de la PlacaCAN
    if ((id == CAN_SPI_ID) && (pcs == CAN_SPI_PCS))
    {
        *pin = PIN_CAN_SPI_PCS0;
        return true;
    }
    return false;
}

static void clockEnable (uint8_t id)
{
    switch (id)
    {
        case 0: SIM->SCGC6 |= SIM_SCGC6_SPI0_MASK; break;
        case 1: SIM->SCGC6 |= SIM_SCGC6_SPI1_MASK; break;
        case 2: SIM->SCGC3 |= SIM_SCGC3_SPI2_MASK; break;
        default: break;
    }
}

// Busca PBR y BR para la mayor frecuencia de SCK que no supere la pedida
static uint32_t baudrateBits (uint32_t baudrate)
{
    uint32_t bestFreq = 0;
    uint32_t bestBits = SPI_CTAR_PBR(3) | SPI_CTAR_BR(15);   // la mas lenta

    for (uint8_t p = 0; p < 4; p++)
    {
        for (uint8_t b = 0; b < 16; b++)
        {
            uint32_t freq = BUS_CLOCK_HZ / ((uint32_t)prescalers[p] * scalers[b]);
            if ((freq <= baudrate) && (freq > bestFreq))
            {
                bestFreq = freq;
                bestBits = SPI_CTAR_PBR(p) | SPI_CTAR_BR(b);
            }
        }
    }
    return bestBits;
}

static inline uint32_t pushrWord (spi_state_t *s, uint8_t index)
{
    uint8_t data = (s->txBuf != NULL) ? s->txBuf[index] : 0x00;
    bool    last = (index == (uint8_t)(s->len - 1));

    return SPI_PUSHR_CONT(!last) | SPI_PUSHR_CTAS(0) |
           SPI_PUSHR_PCS(1U << s->pcs) | SPI_PUSHR_TXDATA(data);
}

static void startFrame (SPI_Type *spi)
{
    spi->MCR |= SPI_MCR_HALT_MASK | SPI_MCR_CLR_TXF_MASK | SPI_MCR_CLR_RXF_MASK;
    spi->SR = SR_CLEAR_ALL;
    spi->MCR &= ~SPI_MCR_HALT_MASK;
}


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

bool spiInit (uint8_t id, const spi_cfg_t *config)
{
    pin_t pcs;

    if ((id >= SPI_CANT_IDS) || (spiPins[id].sck == PIN_NONE) ||
        !pcsPin(id, config->pcs, &pcs))
    {
        return false;
    }

    SPI_Type *spi = spiPtrs[id];
    spi_state_t *s = &spiState[id];

    clockEnable(id);

    // Pull-up en MISO: el esclavo lo deja en Hi-Z sin CS, y si no hay nada
    // conectado se lee 0xFF en vez de ruido
    pinMux(spiPins[id].sck,  spiPins[id].mux, false);
    pinMux(spiPins[id].sout, spiPins[id].mux, false);
    pinMux(spiPins[id].sin,  spiPins[id].mux, true);
    pinMux(pcs,              spiPins[id].mux, false);

    // Master, PCS activos en bajo, FIFOs habilitados, detenido mientras se configura
    spi->MCR = SPI_MCR_MSTR_MASK | SPI_MCR_PCSIS(1U << config->pcs) |
               SPI_MCR_HALT_MASK | SPI_MCR_CLR_TXF_MASK | SPI_MCR_CLR_RXF_MASK;

    // 8 bits. Los retardos de CS (setup, hold y tiempo entre frames) quedan en
    // 4 ciclos de bus = 80ns, por encima de los 50ns que pide el MCP25625.
    spi->CTAR[0] = SPI_CTAR_FMSZ(8 - 1) |
                   SPI_CTAR_CPOL(config->cpol) | SPI_CTAR_CPHA(config->cpha) |
                   SPI_CTAR_LSBFE(config->lsbFirst ? 1 : 0) |
                   SPI_CTAR_PCSSCK(0) | SPI_CTAR_CSSCK(1) |
                   SPI_CTAR_PASC(0)   | SPI_CTAR_ASC(1)   |
                   SPI_CTAR_PDT(0)    | SPI_CTAR_DT(1)    |
                   baudrateBits(config->baudrate);

    spi->SR   = SR_CLEAR_ALL;
    spi->RSER = SPI_RSER_RFDF_RE_MASK;          // interrupcion (no DMA) por RX

    s->pcs   = config->pcs;
    s->busy  = false;
    s->ready = true;

    tpInit();
    NVIC_ClearPendingIRQ(spiIrqs[id]);
    NVIC_SetPriority(spiIrqs[id], SPI_IRQ_PRIORITY);
    NVIC_EnableIRQ(spiIrqs[id]);

    return true;
}

bool spiTransfer (uint8_t id, const uint8_t *txBuf, uint8_t *rxBuf,
                  uint8_t len, spi_callback_t callback)
{
    if ((id >= SPI_CANT_IDS) || !spiState[id].ready || spiState[id].busy)
    {
        return false;
    }

    SPI_Type *spi = spiPtrs[id];
    spi_state_t *s = &spiState[id];

    if (len == 0)
    {
        if (callback != NULL)
        {
            callback();
        }
        return true;
    }

    s->txBuf    = txBuf;
    s->rxBuf    = rxBuf;
    s->len      = len;
    s->txCount  = 0;
    s->rxCount  = 0;
    s->callback = callback;
    s->busy     = true;

    // Con la IRQ enmascarada mientras se llena el FIFO, para que la ISR no lea
    // txCount a mitad de camino y repita un byte
    NVIC_DisableIRQ(spiIrqs[id]);

    startFrame(spi);

    // Se llena el FIFO de TX; el resto lo carga la ISR a medida que llega RX
    while ((s->txCount < s->len) && (s->txCount < fifoDepth[id]))
    {
        spi->PUSHR = pushrWord(s, s->txCount);
        s->txCount++;
    }

    NVIC_EnableIRQ(spiIrqs[id]);

    return true;
}

bool spiTransferBlocking (uint8_t id, const uint8_t *txBuf, uint8_t *rxBuf,
                          uint8_t len)
{
    if ((id >= SPI_CANT_IDS) || !spiState[id].ready || spiState[id].busy)
    {
        return false;
    }

    SPI_Type *spi = spiPtrs[id];
    spi_state_t *s = &spiState[id];

    s->txBuf = txBuf;
    s->len   = len;

    spi->RSER = 0;                              // que la ISR no se robe los bytes
    startFrame(spi);

    for (uint8_t i = 0; i < len; i++)
    {
        spi->PUSHR = pushrWord(s, i);

        while (!(spi->SR & SPI_SR_RFDF_MASK))
        {
        }

        uint8_t data = (uint8_t)spi->POPR;
        spi->SR = SPI_SR_RFDF_MASK;

        if (rxBuf != NULL)
        {
            rxBuf[i] = data;
        }
    }

    spi->SR = SR_CLEAR_ALL;
    NVIC_ClearPendingIRQ(spiIrqs[id]);
    spi->RSER = SPI_RSER_RFDF_RE_MASK;

    return true;
}

bool spiIsBusy (uint8_t id)
{
    return (id < SPI_CANT_IDS) && spiState[id].busy;
}


/*******************************************************************************
 * INTERRUPT SERVICE ROUTINES
 ******************************************************************************/

static void spiIrqHandler (uint8_t id)
{
    SPI_Type *spi = spiPtrs[id];
    spi_state_t *s = &spiState[id];

    while (s->busy && (spi->SR & SPI_SR_RFDF_MASK))
    {
        uint8_t data = (uint8_t)spi->POPR;
        spi->SR = SPI_SR_RFDF_MASK;

        if (s->rxBuf != NULL)
        {
            s->rxBuf[s->rxCount] = data;
        }
        s->rxCount++;

        if (s->txCount < s->len)
        {
            spi->PUSHR = pushrWord(s, s->txCount);
            s->txCount++;
        }
        else if (s->rxCount == s->len)
        {
            // Se libera antes del callback para que pueda encadenar otra
            // transferencia; si lo hace, el while sigue con la nueva
            s->busy = false;
            if (s->callback != NULL)
            {
                s->callback();
            }
        }
    }

    if (!s->busy)
    {
        spi->SR = SPI_SR_RFDF_MASK;             // no deberia haber nada, por las dudas
    }
}

__ISR__ SPI0_IRQHandler (void)
{
    tpIsrEnter();
    spiIrqHandler(0);
    tpIsrExit();
}

__ISR__ SPI1_IRQHandler (void)
{
    tpIsrEnter();
    spiIrqHandler(1);
    tpIsrExit();
}

__ISR__ SPI2_IRQHandler (void)
{
    tpIsrEnter();
    spiIrqHandler(2);
    tpIsrExit();
}
