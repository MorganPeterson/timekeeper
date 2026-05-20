#!/usr/bin/env sh

set -eu

BIN="./timekeeper"

TEST_TMP="${TMPDIR:-/tmp}/timekeeper-test-$$"
TEST_HOME="$TEST_TMP/home"
TEST_XDG="$TEST_TMP/xdg"

mkdir -p "$TEST_HOME" "$TEST_XDG"

cleanup() {
    rm -rf "$TEST_TMP"
    rm -f /tmp/timekeeper-test-out /tmp/timekeeper-test-err
}

trap cleanup EXIT INT TERM

run_clean() {
    env -u TIMEKEEPER_EPOCH HOME="$TEST_HOME" XDG_CONFIG_HOME="$TEST_XDG" "$BIN" "$@"
}

run_with_env_epoch() {
    epoch="$1"
    shift
    env TIMEKEEPER_EPOCH="$epoch" HOME="$TEST_HOME" XDG_CONFIG_HOME="$TEST_XDG" "$BIN" "$@"
}

failures=0

assert_eq() {
    name="$1"
    expected="$2"
    actual="$3"

    if [ "$expected" = "$actual" ]; then
        printf "PASS: %s\n" "$name"
    else
        printf "FAIL: %s\n" "$name"
        printf "  expected: %s\n" "$expected"
        printf "  actual:   %s\n" "$actual"
        failures=$((failures + 1))
    fi
}

assert_fails() {
    name="$1"
    shift

    if "$@" >/tmp/timekeeper-test-out 2>/tmp/timekeeper-test-err; then
        printf "FAIL: %s\n" "$name"
        printf "  expected command to fail, but it succeeded\n"
        failures=$((failures + 1))
    else
        printf "PASS: %s\n" "$name"
    fi
}

assert_eq \
    "encode-date basic example" \
    "2024B04" \
    "$(run_clean encode-date 2024-1-18)"

assert_eq \
    "decode-date basic example" \
    "2024-01-18" \
    "$(run_clean decode-date 2024B04)"

assert_eq \
    "encode-time 06:00:00" \
    "250:000" \
    "$(run_clean encode-time 06:00:00)"

assert_eq \
    "decode-time 250:000" \
    "06:00:00" \
    "$(run_clean decode-time 250:000)"

assert_eq \
    "encode-date first day of year" \
    "2024A01" \
    "$(run_clean encode-date 2024-1-1)"

assert_eq \
    "decode-date first day of year" \
    "2024-01-01" \
    "$(run_clean decode-date 2024A01)"

assert_eq \
    "encode-date 14th day of year" \
    "2024A14" \
    "$(run_clean encode-date 2024-1-14)"

assert_eq \
    "decode-date 14th day of year" \
    "2024-01-14" \
    "$(run_clean decode-date 2024A14)"

assert_eq \
    "encode-date 15th day of year" \
    "2024B01" \
    "$(run_clean encode-date 2024-1-15)"

assert_eq \
    "decode-date 15th day of year" \
    "2024-01-15" \
    "$(run_clean decode-date 2024B01)"

assert_eq \
    "encode-date non-leap final day" \
    "2023+00" \
    "$(run_clean encode-date 2023-12-31)"

assert_eq \
    "decode-date non-leap final day" \
    "2023-12-31" \
    "$(run_clean decode-date 2023+00)"

assert_eq \
    "encode-date leap final day" \
    "2024+01" \
    "$(run_clean encode-date 2024-12-31)"

assert_eq \
    "decode-date leap final day" \
    "2024-12-31" \
    "$(run_clean decode-date 2024+01)"

assert_eq \
    "encode-time midnight" \
    "000:000" \
    "$(run_clean encode-time 00:00:00)"

assert_eq \
    "decode-time midnight" \
    "00:00:00" \
    "$(run_clean decode-time 000:000)"

assert_eq \
    "encode-time noon" \
    "500:000" \
    "$(run_clean encode-time 12:00:00)"

assert_eq \
    "decode-time noon" \
    "12:00:00" \
    "$(run_clean decode-time 500:000)"

assert_eq \
    "encode-time end of day" \
    "999:989" \
    "$(run_clean encode-time 23:59:59)"

assert_eq \
    "decode-time near end of day" \
    "23:59:59" \
    "$(run_clean decode-time 999:989)"

assert_fails \
    "invalid date month" \
    "$BIN" encode-date 2024-13-01

assert_fails \
    "invalid date day" \
    "$BIN" encode-date 2024-02-31

assert_fails \
    "invalid time hour" \
    "$BIN" encode-time 24:00:00

assert_fails \
    "invalid command" \
    "$BIN" nope 2024-01-01

assert_fails \
    "reject year zero" \
    "$BIN" encode-date 0000-01-01

assert_fails \
    "reject huge year overflow" \
    "$BIN" encode-date 999999999999999999999-01-01

assert_fails \
    "reject trailing garbage in date" \
    "$BIN" encode-date 2024-01-01abc

assert_fails \
    "reject trailing garbage in time" \
    "$BIN" encode-time 06:00:00abc

assert_fails \
    "reject invalid timekeeper overflow on non-leap year" \
    "$BIN" decode-date 2023+01

assert_fails \
    "reject impossible timekeeper overflow" \
    "$BIN" decode-date 2024+02

assert_fails \
    "reject timekeeper day zero for normal month" \
    "$BIN" decode-date 2024A00

assert_fails \
    "reject timekeeper day greater than 14" \
    "$BIN" decode-date 2024A15

assert_fails \
    "reject neralie pulse overflow" \
    "$BIN" decode-time 250:1000

assert_fails \
    "reject neralie beat overflow" \
    "$BIN" decode-time 1000:000

assert_eq \
    "epoch encode starts at year zero" \
    "0000A01" \
    "$(run_clean --epoch 1990-05-12 encode-date 1990-05-12)"

assert_eq \
    "epoch encode day 14" \
    "0000A14" \
    "$(run_clean --epoch 1990-05-12 encode-date 1990-05-25)"

assert_eq \
    "epoch encode day 15 starts B" \
    "0000B01" \
    "$(run_clean --epoch 1990-05-12 encode-date 1990-05-26)"

assert_eq \
    "epoch encode final day before anniversary" \
    "0000+00" \
    "$(run_clean --epoch 1990-05-12 encode-date 1991-05-11)"

assert_eq \
    "epoch encode first anniversary" \
    "0001A01" \
    "$(run_clean --epoch 1990-05-12 encode-date 1991-05-12)"

assert_eq \
    "epoch decode start date" \
    "1990-05-12" \
    "$(run_clean --epoch 1990-05-12 decode-date 0000A01)"

assert_eq \
    "epoch decode day 14" \
    "1990-05-25" \
    "$(run_clean --epoch 1990-05-12 decode-date 0000A14)"

assert_eq \
    "epoch decode day 15" \
    "1990-05-26" \
    "$(run_clean --epoch 1990-05-12 decode-date 0000B01)"

assert_eq \
    "epoch decode final day before anniversary" \
    "1991-05-11" \
    "$(run_clean --epoch 1990-05-12 decode-date 0000+00)"

assert_eq \
    "epoch decode first anniversary" \
    "1991-05-12" \
    "$(run_clean --epoch 1990-05-12 decode-date 0001A01)"

assert_eq \
    "epoch equals syntax encode" \
    "0000A01" \
    "$(run_clean --epoch=1990-05-12 encode-date 1990-05-12)"

assert_eq \
    "epoch equals syntax decode" \
    "1990-05-12" \
    "$(run_clean --epoch=1990-05-12 decode-date 0000A01)"

assert_eq \
    "env epoch encode" \
    "0000A01" \
    "$(run_with_env_epoch 1990-05-12 encode-date 1990-05-12)"

assert_eq \
    "env epoch decode" \
    "1990-05-12" \
    "$(run_with_env_epoch 1990-05-12 decode-date 0000A01)"

mkdir -p "$TEST_XDG/timekeeper"
printf "epoch=1990-05-12\n" > "$TEST_XDG/timekeeper/config"

assert_eq \
    "xdg config epoch encode" \
    "0000A01" \
    "$(run_clean encode-date 1990-05-12)"

assert_eq \
    "xdg config epoch decode" \
    "1990-05-12" \
    "$(run_clean decode-date 0000A01)"

assert_eq \
    "cli epoch overrides env epoch" \
    "0000A01" \
    "$(run_with_env_epoch 2000-01-01 --epoch 1990-05-12 encode-date 1990-05-12)"

assert_eq \
    "env epoch overrides config epoch" \
    "0000A01" \
    "$(run_with_env_epoch 2000-01-01 encode-date 2000-01-01)"

assert_eq \
    "no epoch disables configured epoch" \
    "1990J06" \
    "$(run_clean --no-epoch encode-date 1990-05-12)"

assert_eq \
    "feb 29 epoch clamps first anniversary to feb 28" \
    "0001A01" \
    "$(run_clean --epoch 2020-02-29 encode-date 2021-02-28)"

assert_eq \
    "feb 29 epoch decodes clamped first anniversary" \
    "2021-02-28" \
    "$(run_clean --epoch 2020-02-29 decode-date 0001A01)"

assert_eq \
    "feb 29 epoch returns to feb 29 on leap anniversary" \
    "0004A01" \
    "$(run_clean --epoch 2020-02-29 encode-date 2024-02-29)"

assert_eq \
    "feb 29 epoch decodes leap anniversary" \
    "2024-02-29" \
    "$(run_clean --epoch 2020-02-29 decode-date 0004A01)"

assert_fails \
    "epoch rejects date before epoch" \
    run_clean --epoch 1990-05-12 encode-date 1990-05-11

assert_fails \
    "epoch rejects invalid cli epoch" \
    run_clean --epoch 1990-02-31 encode-date 1990-05-12

assert_fails \
    "epoch rejects missing cli epoch value" \
    run_clean --epoch

assert_fails \
    "epoch rejects invalid overflow day" \
    run_clean --epoch 1990-05-12 decode-date 0000+01

if [ "$failures" -gt 0 ]; then
    printf "\n%d test(s) failed.\n" "$failures"
    exit 1
fi

printf "\nAll tests passed.\n"
