#include "oled_port.h"
#include "device_registers.h"

/* S32K144: LPI2C0 SDA=PTA2 ALT3, SCL=PTA3 ALT3.
 * Requires caller to configure system clocks using clocks_and_modes.
 * Bus source: SPLLDIV2_CLK=40MHz in the tested example.
 * Check pin routing and pull-ups on your particular board.
 */
#define PORT_I2C_ERRORS (LPI2C_MSR_NDF_MASK  | \
                         LPI2C_MSR_ALF_MASK  | \
                         LPI2C_MSR_FEF_MASK  | \
                         LPI2C_MSR_PLTF_MASK)
#define PORT_WAIT_LIMIT (1000000u)

void OLED_PortInit(void)
{
    PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
    PORTA->PCR[2] = PORT_PCR_MUX(3);  /* LPI2C0 SDA */
    PORTA->PCR[3] = PORT_PCR_MUX(3);  /* LPI2C0 SCL */

    PCC->PCCn[PCC_LPI2C0_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPI2C0_INDEX] =
        PCC_PCCn_PCS(6) | PCC_PCCn_CGC_MASK;

    LPI2C0->MCR = LPI2C_MCR_RST_MASK;
    LPI2C0->MCR = 0u;

    /* Values preserved from the already-functioning user prototype. */
    LPI2C0->MCFGR1 = LPI2C_MCFGR1_PRESCALE(4);
    LPI2C0->MCCR0 = LPI2C_MCCR0_CLKLO(14) |
                     LPI2C_MCCR0_CLKHI(9) |
                     LPI2C_MCCR0_SETHOLD(8) |
                     LPI2C_MCCR0_DATAVD(4);
    LPI2C0->MFCR = LPI2C_MFCR_TXWATER(0);
    LPI2C0->MCR = LPI2C_MCR_MEN_MASK;
}

static int Port_Put(uint16_t value)
{
    uint32_t limit;
    for (limit = 0u; limit < PORT_WAIT_LIMIT; ++limit)
    {
        uint32_t msr = LPI2C0->MSR;
        if ((msr & PORT_I2C_ERRORS) != 0u)
            return -2;
        if ((msr & LPI2C_MSR_TDF_MASK) != 0u)
        {
            LPI2C0->MTDR = value;
            return 0;
        }
    }
    return -1;
}

int OLED_PortWrite(uint8_t address7, uint8_t control,
                   const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint32_t limit;
    int result;

    if ((data == 0) && (len != 0u))
        return -4;
    if (address7 > 0x7Fu)
        return -4;

    /* Status flags are write-one-to-clear. */
    LPI2C0->MSR = LPI2C_MSR_SDF_MASK | PORT_I2C_ERRORS;

    /* START + 7-bit address + write bit. */
    result = Port_Put(LPI2C_MTDR_CMD(4) |
                      LPI2C_MTDR_DATA((uint32_t)address7 << 1));
    if (result != 0) return result;

    result = Port_Put(LPI2C_MTDR_CMD(0) | LPI2C_MTDR_DATA(control));
    if (result != 0) return result;

    for (i = 0u; i < len; ++i)
    {
        result = Port_Put(LPI2C_MTDR_CMD(0) | LPI2C_MTDR_DATA(data[i]));
        if (result != 0) return result;
    }

    result = Port_Put(LPI2C_MTDR_CMD(2)); /* STOP */
    if (result != 0) return result;

    for (limit = 0u; limit < PORT_WAIT_LIMIT; ++limit)
    {
        uint32_t msr = LPI2C0->MSR;
        if ((msr & PORT_I2C_ERRORS) != 0u)
            return -2;
        if ((msr & LPI2C_MSR_SDF_MASK) != 0u)
        {
            LPI2C0->MSR = LPI2C_MSR_SDF_MASK;
            return 0;
        }
    }
    return -3;
}
