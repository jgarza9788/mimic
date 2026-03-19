#!/usr/bin/env sh
set -eu

PROJECT_ROOT="$1"
SESSION_SCRIPT="${PROJECT_ROOT}/scripts/scrollwm-session"
TMPDIR_TEST="$(mktemp -d)"
trap 'rm -rf "${TMPDIR_TEST}"' EXIT INT TERM

fail() {
  echo "session_wrapper_test: $*" >&2
  exit 1
}

# Case 1: missing scrollwm binary should fail with actionable error.
TEST_PATH="${TMPDIR_TEST}:/usr/bin:/bin"

if PATH="${TEST_PATH}" "${SESSION_SCRIPT}" >/dev/null 2>"${TMPDIR_TEST}/missing.err"; then
  fail "session wrapper unexpectedly succeeded without scrollwm binary"
fi
grep -q "unable to locate 'scrollwm'" "${TMPDIR_TEST}/missing.err" || fail "missing binary error message mismatch"

# Case 2: wait loop should retry ownership errors and eventually succeed.
cat >"${TMPDIR_TEST}/scrollwm" <<'EOF'
#!/usr/bin/env sh
set -eu
STATE_FILE="${TMPDIR:-/tmp}/scrollwm-session-wrapper-state"
COUNT=0
if [ -f "${STATE_FILE}" ]; then
  COUNT=$(cat "${STATE_FILE}")
fi
COUNT=$((COUNT + 1))
echo "${COUNT}" >"${STATE_FILE}"
if [ "${COUNT}" -lt 3 ]; then
  echo "could not acquire WM ownership (is another WM running?)" >&2
  exit 1
fi
exit 0
EOF
chmod +x "${TMPDIR_TEST}/scrollwm"

PATH="${TEST_PATH}" TMPDIR="${TMPDIR_TEST}" \
  "${SESSION_SCRIPT}" --wait-for-wm=3 >/dev/null 2>"${TMPDIR_TEST}/wait.err" || fail "wait loop should have succeeded"

ATTEMPTS="$(cat "${TMPDIR_TEST}/scrollwm-session-wrapper-state")"
[ "${ATTEMPTS}" = "3" ] || fail "expected 3 attempts, got ${ATTEMPTS}"
grep -q "waiting for existing WM to exit (1/3)" "${TMPDIR_TEST}/wait.err" || fail "missing first wait message"
grep -q "waiting for existing WM to exit (2/3)" "${TMPDIR_TEST}/wait.err" || fail "missing second wait message"
