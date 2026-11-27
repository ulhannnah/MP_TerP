#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Test-only -no-pie makes host mock SRAM addresses fit S32K144 32-bit TCD.
gcc -std=c11 -Wall -Wextra -Werror -pedantic -fno-pie -no-pie \
  -DMP_TERP_OLED_EDMA_ENABLE=1 \
  -Itests/oled_mock -Isrc/header \
  src/oled_port_s32k144.c tests/test_oled_port_edma.c \
  -o /tmp/mpterp_oled_port_edma_test
/tmp/mpterp_oled_port_edma_test
