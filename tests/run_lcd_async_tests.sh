#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Itests/lcd_mock -Isrc/header \
  src/lcd_hd44780.c tests/test_lcd_async.c \
  -o /tmp/mpterp_lcd_async_test
/tmp/mpterp_lcd_async_test
