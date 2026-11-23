#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
  src/dsp_fir.c src/coordinate_math.c src/mp_trig.c tests/test_math_dsp.c \
  -o /tmp/mpterp_math_test
/tmp/mpterp_math_test
