/***************************************************************************//**
  @file     mcp25625.c
  @brief    Controlador CAN externo MCP25625 (PlacaCAN) por SPI
  @author   grupo 2

            La configuracion se hace con SPI bloqueante (corre una sola vez,
            con las interrupciones apagadas). Despues todo es por
            interrupciones, con una maquina de estados que avanza al terminar
            cada transferencia SPI:

              READ STATUS -> segun lo que diga:
                RX0IF / RX1IF  -> READ RX BUFFER (borra el flag solo) -> cola RX
                TXnIF          -> BIT MODIFY de CANINTF para borrarlos
                cola TX y algun TXB libre -> LOAD TX BUFFER -> RTS
                nada           -> reposo
              y despues de cada accion vuelve a READ STATUS.

            Se despierta con el flanco de bajada de INT (llego un frame o se
            libero un buffer de TX) o cuando la app encola un frame. Como INT es
            por nivel en el MCP y por flanco en la Kinetis, no se va a reposo
            hasta que no queda ningun flag levantado: si no, INT quedaria en
            bajo y no habria otro flanco que lo despierte.
 ******************************************************************************/

#include "mcp25625.h"
#include "spi.h"
#include "gpio.h"
#include "board.h"
#include "hardware.h"
#include <stddef.h>


/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define MCP_SPI_BAUDRATE    2000000U    // holgado para cables de prototipo (max 10MHz)

// Instrucciones SPI
#define INS_RESET           0xC0
#define INS_READ            0x03
#define INS_WRITE           0x02
#define INS_BIT_MODIFY      0x05
#define INS_READ_STATUS     0xA0
#define INS_READ_RXB(n)     (0x90 | ((n) << 2))     // desde RXBnSIDH
#define INS_LOAD_TXB(n)     (0x40 | ((n) << 1))     // desde TXBnSIDH
#define INS_RTS(n)          (0x80 | (1U << (n)))

// Registros
#define REG_RXF0SIDH        0x00
#define REG_RXF1SIDH        0x04
#define REG_RXF2SIDH        0x08
#define REG_RXF3SIDH        0x10
#define REG_RXF4SIDH        0x14
#define REG_RXF5SIDH        0x18
#define REG_RXM0SIDH        0x20
#define REG_RXM1SIDH        0x24
#define REG_CANSTAT         0x0E
#define REG_CANCTRL         0x0F
#define REG_CNF3            0x28
#define REG_CNF2            0x29
#define REG_CNF1            0x2A
#define REG_CANINTE         0x2B
#define REG_CANINTF         0x2C
#define REG_RXB0CTRL        0x60
#define REG_RXB1CTRL        0x70

// Modos de operacion (CANCTRL.REQOP / CANSTAT.OPMOD, bits 7-5)
#define MODE_MASK           0xE0
#define MODE_NORMAL         0x00
#define MODE_LOOPBACK       0x40
#define MODE_CONFIG         0x80

// CANINTE / CANINTF
#define INT_RX0             0x01
#define INT_RX1             0x02
#define INT_TX0             0x04
#define INT_TX1             0x08
#define INT_TX2             0x10

// Respuesta de READ STATUS
#define ST_RX0IF            0x01
#define ST_RX1IF            0x02
#define ST_TXREQ(n)         (0x04 << (2 * (n)))
#define ST_TXIF(n)          (0x08 << (2 * (n)))
#define ST_TXIF_ALL         (ST_TXIF(0) | ST_TXIF(1) | ST_TXIF(2))

#define RXB0CTRL_BUKT       0x04        // si RXB0 esta ocupado, el frame pasa a RXB1
#define SIDL_IDE            0x08        // frame extendido
#define SIDL_SRR            0x10        // remote frame (estandar)
#define DLC_MASK            0x0F

// Bit timing: 16 TQ por bit, punto de muestreo al 75%
#define TQ_PER_BIT          16
#define PROPSEG             5
#define PHSEG1              6
#define PHSEG2              4           // 1 + 5 + 6 + 4 = 16
#define SJW                 1

#define CNF2_BTLMODE        0x80        // PHSEG2 lo define CNF3

// Frame en un buffer: SIDH SIDL EID8 EID0 DLC D0..D7
#define BUF_HEADER_LEN      5
#define BUF_LEN             (BUF_HEADER_LEN + CAN_MAX_DLC)

#define CONFIG_TIMEOUT      10000       // lecturas de CANSTAT antes de rendirse


/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef enum {
    ST_IDLE,
    ST_STATUS,
    ST_READ_RX,
    ST_CLEAR_TXIF,
    ST_LOAD_TX,
    ST_RTS,
} mcp_state_t;


/*******************************************************************************
 * VARIABLES WITH LOCAL SCOPE
 ******************************************************************************/

static bool ready = false;

static volatile mcp_state_t state = ST_IDLE;
static volatile bool        wakePending = false;
static uint8_t              txbInUse;               // TXB que se esta cargando

// Una sola transferencia en vuelo a la vez: alcanza con un par de buffers
static uint8_t spiTx[1 + BUF_LEN];
static uint8_t spiRx[1 + BUF_LEN];

static can_frame_t       txQueue[MCP_TX_QUEUE_SIZE];
static volatile uint8_t  txHead = 0, txTail = 0;    // head: app, tail: ISR

static can_frame_t       rxQueue[MCP_RX_QUEUE_SIZE];
static volatile uint8_t  rxHead = 0, rxTail = 0;    // head: ISR, tail: app
static volatile uint32_t rxDropped = 0;


/*******************************************************************************
 * LOCAL FUNCTION DECLARATIONS
 ******************************************************************************/

static void startStatus (void);
static void onSpiDone (void);


/*******************************************************************************
 * LOCAL FUNCTION DEFINITIONS: acceso bloqueante, solo para la configuracion
 ******************************************************************************/

static void writeRegs (uint8_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t buf[2 + 4];

    buf[0] = INS_WRITE;
    buf[1] = addr;
    for (uint8_t i = 0; i < len; i++)
    {
        buf[2 + i] = data[i];
    }
    spiTransferBlocking(CAN_SPI_ID, buf, NULL, 2 + len);
}

static void writeReg (uint8_t addr, uint8_t value)
{
    writeRegs(addr, &value, 1);
}

static uint8_t readReg (uint8_t addr)
{
    uint8_t tx[3] = { INS_READ, addr, 0x00 };
    uint8_t rx[3];

    spiTransferBlocking(CAN_SPI_ID, tx, rx, 3);
    return rx[2];
}

static bool waitMode (uint8_t mode)
{
    for (uint32_t i = 0; i < CONFIG_TIMEOUT; i++)
    {
        if ((readReg(REG_CANSTAT) & MODE_MASK) == mode)
        {
            return true;
        }
    }
    return false;
}

// ID estandar en el formato SIDH/SIDL/EID8/EID0 de filtros, mascaras y buffers
static void packStdId (uint16_t id, uint8_t *dst)
{
    dst[0] = (uint8_t)(id >> 3);
    dst[1] = (uint8_t)((id & 0x07) << 5);
    dst[2] = 0;
    dst[3] = 0;
}

static void writeId (uint8_t addr, uint16_t id)
{
    uint8_t buf[4];

    packStdId(id, buf);
    writeRegs(addr, buf, 4);
}


/*******************************************************************************
 * LOCAL FUNCTION DEFINITIONS: maquina de estados (corre en ISR)
 ******************************************************************************/

static void startStatus (void)
{
    wakePending = false;        // lo que pase de aca en adelante lo va a ver este READ STATUS
    state = ST_STATUS;
    spiTx[0] = INS_READ_STATUS;
    spiTx[1] = 0x00;
    spiTransfer(CAN_SPI_ID, spiTx, spiRx, 2, onSpiDone);
}

static void startReadRx (uint8_t n)
{
    state = ST_READ_RX;
    spiTx[0] = INS_READ_RXB(n);
    for (uint8_t i = 1; i <= BUF_LEN; i++)
    {
        spiTx[i] = 0x00;
    }
    spiTransfer(CAN_SPI_ID, spiTx, spiRx, 1 + BUF_LEN, onSpiDone);
}

static void startClearTxIf (uint8_t status)
{
    uint8_t mask = 0;

    for (uint8_t n = 0; n < 3; n++)
    {
        if (status & ST_TXIF(n))
        {
            mask |= (INT_TX0 << n);
        }
    }

    state = ST_CLEAR_TXIF;
    spiTx[0] = INS_BIT_MODIFY;
    spiTx[1] = REG_CANINTF;
    spiTx[2] = mask;
    spiTx[3] = 0x00;
    spiTransfer(CAN_SPI_ID, spiTx, NULL, 4, onSpiDone);
}

static void startLoadTx (uint8_t n)
{
    const can_frame_t *f = &txQueue[txTail];
    uint8_t len = (f->len > CAN_MAX_DLC) ? CAN_MAX_DLC : f->len;

    spiTx[0] = INS_LOAD_TXB(n);
    packStdId(f->id, &spiTx[1]);
    spiTx[5] = len;
    for (uint8_t i = 0; i < len; i++)
    {
        spiTx[1 + BUF_HEADER_LEN + i] = f->data[i];
    }

    txTail = (uint8_t)((txTail + 1) % MCP_TX_QUEUE_SIZE);

    txbInUse = n;
    state = ST_LOAD_TX;
    spiTransfer(CAN_SPI_ID, spiTx, NULL, 1 + BUF_HEADER_LEN + len, onSpiDone);
}

static void startRts (void)
{
    state = ST_RTS;
    spiTx[0] = INS_RTS(txbInUse);
    spiTransfer(CAN_SPI_ID, spiTx, NULL, 1, onSpiDone);
}

static void storeRx (void)
{
    const uint8_t *b = &spiRx[1];

    // Solo frames de datos estandar: los extendidos y los remotos no son del TP
    if (b[1] & (SIDL_IDE | SIDL_SRR))
    {
        return;
    }

    uint8_t next = (uint8_t)((rxHead + 1) % MCP_RX_QUEUE_SIZE);
    if (next == rxTail)
    {
        rxDropped++;
        return;
    }

    can_frame_t *f = &rxQueue[rxHead];
    f->id  = (uint16_t)(((uint16_t)b[0] << 3) | (b[1] >> 5));
    f->len = b[4] & DLC_MASK;
    if (f->len > CAN_MAX_DLC)
    {
        f->len = CAN_MAX_DLC;
    }
    for (uint8_t i = 0; i < f->len; i++)
    {
        f->data[i] = b[BUF_HEADER_LEN + i];
    }

    rxHead = next;
}

static void decide (uint8_t status)
{
    if (status & ST_RX0IF)
    {
        startReadRx(0);
    }
    else if (status & ST_RX1IF)
    {
        startReadRx(1);
    }
    else if (status & ST_TXIF_ALL)
    {
        startClearTxIf(status);
    }
    else
    {
        if (txTail != txHead)
        {
            for (uint8_t n = 0; n < 3; n++)
            {
                if (!(status & ST_TXREQ(n)))
                {
                    startLoadTx(n);
                    return;
                }
            }
        }

        // Nada para hacer. Si algo paso despues del ultimo READ STATUS, o INT
        // sigue en bajo, se vuelve a mirar en vez de dormir.
        if (wakePending || (gpioRead(PIN_CAN_INT) == CAN_INT_ACTIVE))
        {
            startStatus();
        }
        else
        {
            state = ST_IDLE;
        }
    }
}

static void onSpiDone (void)
{
    switch (state)
    {
        case ST_STATUS:
            decide(spiRx[1]);
            break;

        case ST_READ_RX:
            storeRx();
            startStatus();
            break;

        case ST_LOAD_TX:
            startRts();
            break;

        case ST_CLEAR_TXIF:
        case ST_RTS:
        default:
            startStatus();
            break;
    }
}

// Lo llaman la ISR de INT y la app (con interrupciones enmascaradas)
static void wake (void)
{
    if (state == ST_IDLE)
    {
        startStatus();
    }
    else
    {
        wakePending = true;
    }
}

static void onIntFalling (void)
{
    wake();
}


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

bool mcpInit (const mcp_cfg_t *config)
{
    ready = false;

    // BRP: TQ = 2 * (BRP + 1) / FOSC
    uint32_t div = 2U * TQ_PER_BIT * config->bitrate;
    if ((config->bitrate == 0) || (CAN_OSC_HZ % div) != 0 || (CAN_OSC_HZ / div) > 64)
    {
        return false;
    }
    uint8_t brp = (uint8_t)(CAN_OSC_HZ / div - 1);

    gpioMode(PIN_CAN_RESET, OUTPUT);
    gpioWrite(PIN_CAN_RESET, !CAN_RESET_ACTIVE);
    gpioMode(PIN_CAN_INT, INPUT_PULLUP);

    spi_cfg_t spiCfg = {
        .baudrate = MCP_SPI_BAUDRATE,
        .cpol     = CAN_SPI_CPOL,
        .cpha     = CAN_SPI_CPHA,
        .pcs      = CAN_SPI_PCS,
        .lsbFirst = false,
    };
    if (!spiInit(CAN_SPI_ID, &spiCfg))
    {
        return false;
    }

    // Reset por SPI: queda en modo configuracion
    uint8_t ins = INS_RESET;
    spiTransferBlocking(CAN_SPI_ID, &ins, NULL, 1);
    if (!waitMode(MODE_CONFIG))
    {
        return false;
    }

    uint8_t cnf[3] = {
        (uint8_t)(((PHSEG2 - 1) & 0x07)),                                       // CNF3
        (uint8_t)(CNF2_BTLMODE | ((PHSEG1 - 1) << 3) | (PROPSEG - 1)),          // CNF2
        (uint8_t)(((SJW - 1) << 6) | brp),                                      // CNF1
    };
    writeRegs(REG_CNF3, cnf, 3);

    // Se relee la configuracion: confirma que el SPI anda en los dos sentidos
    if ((readReg(REG_CNF3) != cnf[0]) || (readReg(REG_CNF2) != cnf[1]) ||
        (readReg(REG_CNF1) != cnf[2]))
    {
        return false;
    }

    // Las dos mascaras iguales y los 6 filtros iguales: los dos buffers de RX
    // aceptan exactamente lo mismo
    writeId(REG_RXM0SIDH, config->filterMask);
    writeId(REG_RXM1SIDH, config->filterMask);
    writeId(REG_RXF0SIDH, config->filterId);
    writeId(REG_RXF1SIDH, config->filterId);
    writeId(REG_RXF2SIDH, config->filterId);
    writeId(REG_RXF3SIDH, config->filterId);
    writeId(REG_RXF4SIDH, config->filterId);
    writeId(REG_RXF5SIDH, config->filterId);

    writeReg(REG_RXB0CTRL, RXB0CTRL_BUKT);
    writeReg(REG_RXB1CTRL, 0x00);

    writeReg(REG_CANINTF, 0x00);
    writeReg(REG_CANINTE, INT_RX0 | INT_RX1 | INT_TX0 | INT_TX1 | INT_TX2);

    // Modo de operacion (sin CLKOUT, sin one-shot)
    uint8_t mode = config->loopback ? MODE_LOOPBACK : MODE_NORMAL;
    writeReg(REG_CANCTRL, mode);
    if (!waitMode(mode))
    {
        return false;
    }

    txHead = txTail = 0;
    rxHead = rxTail = 0;
    rxDropped = 0;
    state = ST_IDLE;
    wakePending = false;

    GPIO_Interrupt(PIN_CAN_INT, IRQ_FALLING, onIntFalling);

    ready = true;
    return true;
}

bool mcpSend (const can_frame_t *frame)
{
    if (!ready)
    {
        return false;
    }

    uint8_t next = (uint8_t)((txHead + 1) % MCP_TX_QUEUE_SIZE);
    if (next == txTail)
    {
        return false;
    }

    txQueue[txHead] = *frame;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    txHead = next;
    wake();
    __set_PRIMASK(primask);

    return true;
}

bool mcpReceive (can_frame_t *frame)
{
    if (rxTail == rxHead)
    {
        return false;
    }

    *frame = rxQueue[rxTail];
    rxTail = (uint8_t)((rxTail + 1) % MCP_RX_QUEUE_SIZE);

    return true;
}

uint32_t mcpGetRxDropped (void)
{
    return rxDropped;
}
