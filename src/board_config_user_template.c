/*
 * Editable hardware mapping scaffold.
 *
 * Enabled ONLY with MP_TERP_USE_BOARD_CONFIG_TEMPLATE.
 * Must be filled from actual MCU board schematic and connector wiring.
 * Unconfigured mapping returns false: no board peripherals are enabled.
 */
#if defined(MP_TERP_USE_BOARD_CONFIG_TEMPLATE) && (defined(MP_TERP_ALL_TEST_MAIN) || (defined(MP_TERP_STAGE_MAIN) && MP_TERP_STAGE != 4))
#include "board_system.h"
#include <stddef.h>
#include <string.h>

bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg)
{
    if(cfg==NULL)return false;
    (void)memset(cfg,0,sizeof(*cfg));

    /*
     * Verify these before changing return false -> true:
     *
     * cfg->timer.pcc_clock_source = <verified LPIT PCS>;
     * cfg->timer.clock_hz         = <verified functional clock in Hz>;
     * cfg->timer.tick_channel     = TIMER_CH3; // EXAMPLE ONLY
     *
     * Stage 2 additionally requires:
     * cfg->servo.ftm, channel, port, port_pcc_index, pin, pin_alt,
     * clock_pcs, clock_hz, period_us, min/max pulse, min/max angle,
     * initial_angle_mdeg.
     *
     * Stage 3 additionally requires:
     * cfg->ultrasonic.ftm, ftm_channel, ftm_clock_pcs/hz,
     * echo_port/pcc_index/pin/pin_alt,
     * trigger_port/gpio/pcc_index/pin,
     * trigger_timer_channel, trigger_us, timeout_us,
     * minimum_echo_us, maximum_echo_us.
     * Connect actual FTM channel-pair IRQ to
     * MP_TERP_StageEchoCaptureIrq() using verified startup vector.
     *
     * Stage 5 requires all of the above and Stepper, ADC, OLED,
     * LCD and LED board interface settings, plus mechanically verified
     * ultrasound-direction/servo-angle coupling.
     *
     * Do NOT fill unknown pins with example or guessed values.
     */

    return false; /* Fail closed until the wiring is known and verified. */
}
#if defined(MP_TERP_ALL_TEST_MAIN)
/* Actual LCD driver initialization must happen after system clock setup.
 * This fail-closed placeholder must be implemented for the physical LCD. */
bool MP_TERP_AllTestLCDInit(void)
{
    return false;
}
#endif
#endif
