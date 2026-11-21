
#include "ADC.h"

/*
 * Clock assumption: SOSCDIV2 = 8 MHz (clocks_and_modes.c).
 * ADC0 uses SOSCDIV2 / ADIV(1) = 8 MHz.
 * Call SOSC_init_8MHz() before initializing ADC0.
 */

static void ADC_configure_clock(void)
{
    uint32_t pcc = PCC->PCCn[PCC_ADC0_INDEX];

    pcc &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_ADC0_INDEX] = pcc;

    pcc &= ~PCC_PCCn_PCS_MASK;
    pcc |= PCC_PCCn_PCS(1);  /* SOSCDIV2 */

    PCC->PCCn[PCC_ADC0_INDEX] =
        pcc | PCC_PCCn_CGC_MASK;
}

static void ADC_configure_common(void)
{
    ADC_configure_clock();

    ADC0->SC1[0] = ADC_SC1_ADCH(31);

    ADC0->CFG1 = ADC_CFG1_ADIV(0)
               | ADC_CFG1_MODE(1);

    ADC0->CFG2 = ADC_CFG2_SMPLTS(12);

    ADC0->SC2 = 0u;
    ADC0->SC3 = 0u;
}

void ADC_init(void)
{
    ADC_configure_common();
}

void ADC_init_HWTrigger(char Channel)
{
    ADC_configure_common();

    /*
     * External trigger routing and NVIC
     * configuration must be done separately.
     */
    ADC0->SC2 = ADC_SC2_ADTRG_MASK;

    ADC0->SC1[0] =
        ADC_SC1_ADCH((uint8_t)Channel)
        | ADC_SC1_AIEN_MASK;
}

void convertAdcChan(uint16_t adcChan)
{
    /* SW trigger */
    ADC0->SC1[0] = ADC_SC1_ADCH(adcChan);
}

uint8_t adc_complete(void)
{
    return ((ADC0->SC1[0] &
             ADC_SC1_COCO_MASK) != 0u) ? 1u : 0u;
}

uint16_t read_adc_raw(void)
{
    /* 12-bit ADC result */
    return (uint16_t)(ADC0->R[0] & 0x0FFFu);
}

uint32_t read_adc_chx(void)
{
    uint32_t raw = (uint32_t)read_adc_raw();

    return (raw * ADC_VREF_MV) / 4095u;
}
