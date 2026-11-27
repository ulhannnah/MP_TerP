#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc/header \
  src/oled.c src/oled_font5x7.c tests/test_oled_traffic_sim.c \
  -o /tmp/mpterp_oled_traffic_sim
/tmp/mpterp_oled_traffic_sim
