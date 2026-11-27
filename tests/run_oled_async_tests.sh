#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
  src/oled.c src/oled_font5x7.c tests/test_oled_async.c \
  -o /tmp/mpterp_oled_async_test
/tmp/mpterp_oled_async_test
