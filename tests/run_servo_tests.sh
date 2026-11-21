#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Itests/servo_mock -Isrc/header \
  src/servo.c tests/test_servo.c -o /tmp/mpterp_servo_test
/tmp/mpterp_servo_test
