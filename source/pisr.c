/***************************************************************************//**
  @file     pisr.c
  @brief    Periodic Interrupt (PISR) driver
  @author   Nicolás Magliola
 ******************************************************************************/

#include "pisr.h"

#include "hardware.h"
#include "gpio.h"
#include <stddef.h>

// Clock del core, igual al que usa el proyecto que anda (systick.c), en vez
// de depender de __CORE_CLOCK__ (viene de hardware.h, no siempre confiable).
#define CORE_CLOCK_HZ        100000000UL

#define SYSTICK_LOAD_INIT   ((CORE_CLOCK_HZ/SYSTICK_ISR_FREQUENCY_HZ) - 1U)


typedef struct {
    bool used;
    unsigned int count;
    unsigned int period;
    pisr_callback_t callback;
} pisr_t;


static pisr_t pisr[PISR_CANT];
static unsigned int pisr_counter = 0;
static volatile uint32_t msCounter = 0;
static bool systickStarted = false;


static void systickStart (void)
{
    SysTick->CTRL = 0x00;
    SysTick->LOAD = SYSTICK_LOAD_INIT;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    NVIC_SetPriority(SysTick_IRQn, 1);
    NVIC_EnableIRQ(SysTick_IRQn);
}


void pisrInit (void)
{
    if (!systickStarted)
    {
        tpInit();       // el TP tiene que estar listo antes del primer tick

        systickStart();
        systickStarted = true;
    }
}


bool pisrRegister (pisr_callback_t fun, unsigned int period)
{
    if (fun == NULL || period == 0 || pisr_counter >= PISR_CANT)
    {
        return false;
    }

    pisrInit();

    pisr[pisr_counter].callback = fun;
    pisr[pisr_counter].period   = period;
    pisr[pisr_counter].count    = period;
    pisr[pisr_counter].used     = true;
    pisr_counter++;

    return true;
}


uint32_t millis (void)
{
    return msCounter;
}


__ISR__ SysTick_Handler (void)
{
    unsigned int i;

    // tpIsrEnter();                   // TP: arranca el tiempo en ISR

    msCounter++;

    for (i = 0; i < pisr_counter; i++)
    {
        if (--pisr[i].count == 0)
        {
            pisr[i].count = pisr[i].period;
            pisr[i].callback();
        }
    }

    // tpIsrExit();                    // TP: fin del tiempo en ISR
}
