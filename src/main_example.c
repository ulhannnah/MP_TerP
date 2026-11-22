/* Optional standalone entry point for S32 Design Studio.
 * Compile with MP_TERP_USE_EXAMPLE_MAIN only if the project has no other main.
 * Application owner supplies MP_TERP_GetBoardConfig(), including the board's
 * *verified* pin/clock resources and LCD/LED callbacks.
 */
#if defined(MP_TERP_USE_EXAMPLE_MAIN)
#include "board_system.h"
static BoardSystem s_board;
static BoardSystemConfig s_config;
extern bool MP_TERP_GetBoardConfig(BoardSystemConfig *config);

int main(void)
{
    if(!MP_TERP_GetBoardConfig(&s_config)||
       !BoardSystem_Init(&s_board,&s_config)) {
        for(;;) { /* integration fault: inspect debugger */ }
    }
    /* StepperController_SetHome(&s_board.stepper, reference_mdeg)
     * must be called only after the mechanical reference is confirmed.
     */
    for(;;) BoardSystem_Update(&s_board);
}
/* Selected FTM channel-pair IRQ handler must invoke:
 * BoardSystem_OnEchoCaptureIrq(&s_board);
 * Selected PORTx IRQ handler must invoke:
 * BoardSystem_OnFireButtonPortIrq(&s_board);
 * Do NOT define generic IRQ handlers here; the correct vector depends on
 * the FTM instance, channel pair, PORT and existing S32DS SDK handlers.
 */
#endif
