#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Itests/stepper_mock -Isrc/header \
  src/stepper_controller.c tests/stepper_mock/mock_timer.c \
  tests/stepper_mock/mock_step_motor.c tests/test_stepper_controller.c \
  -o /tmp/mpterp_stepper_test
/tmp/mpterp_stepper_test
