#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
 src/ultrasonic.c src/radar_scan.c src/distance_filter.c \
 src/background_filter.c src/target_detector.c src/target_tracker.c \
 src/target_selector.c tests/test_radar_pipeline.c -o /tmp/mpterp_pipeline_test
/tmp/mpterp_pipeline_test
