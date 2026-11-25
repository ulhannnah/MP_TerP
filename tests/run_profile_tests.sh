#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/header \
  tests/test_app_profile.c -o /tmp/mpterp_app_profile_test
/tmp/mpterp_app_profile_test
