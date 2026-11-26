#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
  tests/test_final_entry.c -o /tmp/mpterp_final_entry_test
/tmp/mpterp_final_entry_test
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
  -DMP_TERP_ALL_TEST_MAIN -DMP_TERP_TEST_BENCH \
  tests/test_final_entry.c -o /tmp/mpterp_final_bench_test
/tmp/mpterp_final_bench_test
echo 'PASS: Stage5 final default, opt-in ALL_TEST and explicit mechanical guard'
