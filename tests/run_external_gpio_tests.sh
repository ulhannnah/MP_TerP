#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Itests/gpio_mock -Isrc/header \
  src/gpio_io.c tests/test_external_gpio.c -o /tmp/mpterp_external_gpio
/tmp/mpterp_external_gpio
