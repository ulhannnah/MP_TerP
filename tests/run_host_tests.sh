#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Itests -Isrc/header src/timer_manager.c tests/test_timer_manager.c -o /tmp/mp_terp_timer_test
/tmp/mp_terp_timer_test
