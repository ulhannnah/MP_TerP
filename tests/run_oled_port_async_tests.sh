#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Itests/oled_mock -Isrc/header \
  src/oled_port_s32k144.c tests/test_oled_port_async.c \
  -o /tmp/mpterp_oled_port_async_test
/tmp/mpterp_oled_port_async_test
