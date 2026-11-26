#include "project_config.h"

#if defined(MP_TERP_TEST_BENCH)
#ifndef MP_TERP_ALL_TEST_MAIN
#error "Explicit ALL_TEST must stay selectable on final branch"
#endif
#ifdef MP_TERP_STAGE_MAIN
#error "ALL_TEST and Stage5 cannot both own main()"
#endif
#else
#ifndef MP_TERP_STAGE_MAIN
#error "Final branch must enter integrated Stage5 by default"
#endif
#if MP_TERP_STAGE != 5
#error "Default stage must be Stage5"
#endif
#ifndef MP_TERP_USE_BOARD_CONFIG_TEMPLATE
#error "Stage5 must use the established 2026-11-25 pin map"
#endif
#ifndef MP_TERP_CONFIRM_PINMAP
#error "Final branch must use the confirmed application pin assignment"
#endif
#ifdef MP_TERP_CONFIRM_COUPLED_GEOMETRY
#error "Physical geometry confirmation must never be auto-defined"
#endif
#endif

int main(void){return 0;}
