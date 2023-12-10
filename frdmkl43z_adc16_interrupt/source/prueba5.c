/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "board.h"
#include "fsl_adc16.h"

#include "pin_mux.h"
#include "clock_config.h"
#include "SD2_board.h"
#include "key.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define ADC16_BASE          ADC0
#define DEMO_ADC16_CHANNEL_GROUP 0U
#define DEMO_ADC16_USER_CHANNEL  23U /* PTE30, ADC0_SE23 */

#define ADC16_IRQn             ADC0_IRQn

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
const float k_instr = 0.28125f;

volatile int last_conversion_value;
volatile int tara;

volatile bool new_measurement_available = false;

volatile uint32_t timer = 0;

adc16_channel_config_t adc16ChannelConfigStruct;


/*******************************************************************************
 * Code
 ******************************************************************************/

void ADC0_IRQHandler(void)
{
    /* Read conversion result to clear the conversion completed flag. */
	static int cant_conversiones = 0;
	static uint32_t acumulacion = 0;

	acumulacion += ADC16_GetChannelConversionValue(ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP);
    cant_conversiones ++;

    if(cant_conversiones >= 10)
    {
    	last_conversion_value = acumulacion / 10;
    	acumulacion = 0;
    	cant_conversiones = 0;
    	new_measurement_available = true;
    }
}

int main(void)
{
    adc16_config_t adc16ConfigStruct;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    EnableIRQ(ADC16_IRQn);

    board_init();
    key_init();

    SysTick_Config(SystemCoreClock / 1000U);

    PRINTF("\r\nBALANZA DIGITAL - Made by Raffagnini & Rodriguez Inc.\r\n");

    ADC16_GetDefaultConfig(&adc16ConfigStruct);

    adc16ConfigStruct.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
    adc16ConfigStruct.clockSource = kADC16_ClockSourceAlt1; // BUSCLK / 2
    adc16ConfigStruct.enableAsynchronousClock = true;
    adc16ConfigStruct.clockDivider = kADC16_ClockDivider8;
    adc16ConfigStruct.resolution = kADC16_ResolutionSE12Bit;
    adc16ConfigStruct.longSampleMode = kADC16_LongSampleCycle24;
    adc16ConfigStruct.enableHighSpeed = false;
    adc16ConfigStruct.enableLowPower = false;
    adc16ConfigStruct.enableContinuousConversion = false;

    ADC16_Init(ADC16_BASE, &adc16ConfigStruct);
    ADC16_EnableHardwareTrigger(ADC16_BASE, false); /* Make sure the software trigger is used. */

    adc16ChannelConfigStruct.channelNumber                        = DEMO_ADC16_USER_CHANNEL;
    adc16ChannelConfigStruct.enableInterruptOnConversionCompleted = true; /* Enable the interrupt. */

	// Hardware Average Enable
	ADC0->SC3 |= 1 << 2; // Hardware average function enabled.

	// Hardware Average Select
	ADC0->SC3 |= 0B11; // 32 samples averaged

    ADC16_SetChannelConfig(ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP, &adc16ChannelConfigStruct);

    while (1){

        if(key_getPressEv(BOARD_SW_ID_3))
        {
        	DisableIRQ(ADC16_IRQn);

            tara = last_conversion_value;
            PRINTF("Se tara la balanza. ADC mide: %d\r\n", tara);

            EnableIRQ(ADC16_IRQn);
        }

        if(new_measurement_available) {
            DisableIRQ(ADC16_IRQn);

            int peso = (int)((last_conversion_value - tara) * k_instr);
            int resto = peso % 10;
            int peso_normalizado = (resto > 5) ? (peso + 10 - resto) : (peso - resto);
            PRINTF("Peso: %d gramos\r\n",(peso > 0) ? peso : 0);
            PRINTF("Peso normalizado: %d gramos\r\n",(peso_normalizado > 0) ? peso_normalizado : 0);
            new_measurement_available = false;

            EnableIRQ(ADC16_IRQn);
        }
    }
}

void SysTick_Handler(void)
{

    if (timer == 50) {
        timer = 0;
        ADC16_SetChannelConfig(ADC16_BASE, DEMO_ADC16_CHANNEL_GROUP, &adc16ChannelConfigStruct);
    }

    timer++;

	key_periodicTask1ms();
}
