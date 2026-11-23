#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
  src/radar_frame.c src/radar_processing.c src/distance_filter.c \
  src/background_filter.c src/target_detector.c src/target_tracker.c \
  src/radar_scan.c src/target_selector.c src/aim_controller.c \
  src/task_scheduler.c src/radar_renderer.c src/mp_trig.c src/lcd_status.c \
  src/launcher.c src/app.c tests/test_integration.c \
  -o /tmp/mpterp_integration_test
/tmp/mpterp_integration_test
