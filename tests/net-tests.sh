#!/bin/sh
# Network test suite for vp: exercises the http/https fetch path against a
# local server (tests/testserv.py) - redirects, chunked bodies, IPv6, TLS
# certificate handling, and the failure messages. Entirely self-contained;
# never touches the real network.
#
# usage: net-tests.sh <path-to-vp-binary>
# Needs python3 and the openssl command-line tool.

set -u

case "${1-}" in
    '') echo "usage: $0 <path-to-vp-binary>" >&2; exit 2 ;;
    /*) VP=$1 ;;
    *)  VP=$PWD/$1 ;;
esac
[ -x "$VP" ] || { echo "$VP: not an executable" >&2; exit 2; }

TESTS_DIR=$(cd "$(dirname "$0")" && pwd)
HP=18099   # http 127.0.0.1
SP=18443   # https 127.0.0.1
V6=18086   # http [::1]

TMP=$(mktemp -d) || exit 2
SRV_PID=
cleanup () {
    [ -n "$SRV_PID" ] && kill "$SRV_PID" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

# make failures self-explaining rather than another round of guessing
echo "vp:      $VP"
echo "python3: $(python3 --version 2>&1)"
echo "openssl: $(openssl version 2>&1)"

die () {
    echo "$1" >&2
    if [ -f "$TMP/srv.out" ]; then
	echo "--- server log: ---" >&2
	sed 's/^/    /' "$TMP/srv.out" >&2
    fi
    exit 2
}

# portable timeout: macOS ships no timeout(1)
tmo () {
    python3 -c '
import subprocess, sys
try:
    sys.exit(subprocess.run(sys.argv[2:], timeout=float(sys.argv[1])).returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
' "$@"
}

# a small valid PPM to serve
python3 -c '
import sys
sys.stdout.buffer.write(b"P6\n32 32\n255\n" + b"\x40\x80\xc0" * (32 * 32))
' > "$TMP/test1.ppm"

openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=localhost" \
    -keyout "$TMP/key.pem" -out "$TMP/cert.pem" 2>/dev/null || \
    die "openssl could not generate a test certificate"

python3 "$TESTS_DIR/testserv.py" --dir "$TMP" --http-port $HP \
    --https-port $SP --v6-port $V6 --cert "$TMP/cert.pem" \
    --key "$TMP/key.pem" > "$TMP/srv.out" 2>&1 &
SRV_PID=$!
i=0
while [ $i -lt 100 ]; do
    grep -q READY "$TMP/srv.out" 2>/dev/null && break
    kill -0 "$SRV_PID" 2>/dev/null || die "test server died before ready"
    sleep 0.2; i=$((i + 1))
done
grep -q READY "$TMP/srv.out" || die "test server never signalled ready (20s)"

PASS=0
FAIL=0

# run_vp <timeout> <args...>; output lands in $TMP/out, exit code in $RC
run_vp () {
    t=$1; shift
    SDL_VIDEODRIVER=dummy tmo "$t" "$VP" "$@" > "$TMP/out" 2>&1
    RC=$?
}

# a display test succeeds by running until the timeout kills it
check_shows () {
    name=$1; shift
    run_vp 3 "$@"
    if [ "$RC" -eq 124 ]; then
	PASS=$((PASS + 1)); echo "PASS: $name"
    else
	FAIL=$((FAIL + 1)); echo "FAIL: $name (exit $RC)"; sed 's/^/    /' "$TMP/out"
    fi
}

# a message test succeeds by exiting on its own with the expected text
check_says () {
    name=$1; expect=$2; shift 2
    run_vp 6 "$@"
    if [ "$RC" -ne 124 ] && grep -q "$expect" "$TMP/out"; then
	PASS=$((PASS + 1)); echo "PASS: $name"
    else
	FAIL=$((FAIL + 1)); echo "FAIL: $name (exit $RC, wanted \"$expect\")"
	sed 's/^/    /' "$TMP/out"
    fi
}

# says AND keeps displaying (warning + success)
check_says_shows () {
    name=$1; expect=$2; shift 2
    run_vp 3 "$@"
    if [ "$RC" -eq 124 ] && grep -q "$expect" "$TMP/out"; then
	PASS=$((PASS + 1)); echo "PASS: $name"
    else
	FAIL=$((FAIL + 1)); echo "FAIL: $name (exit $RC, wanted \"$expect\")"
	sed 's/^/    /' "$TMP/out"
    fi
}

check_shows "plain http fetch"        "http://127.0.0.1:$HP/test1.ppm"
check_shows "redirect chain"          "http://127.0.0.1:$HP/r1"
check_shows "relative redirect"       "http://127.0.0.1:$HP/rel"
check_shows "protocol-relative"       "http://127.0.0.1:$HP/proto"
check_shows "chunked body"            "http://127.0.0.1:$HP/chunked"
check_shows "bare host"               "http://127.0.0.1:$HP"
if grep -q "V6 ok" "$TMP/srv.out"; then
    check_shows "ipv6 literal"        "http://[::1]:$V6/test1.ppm"
else
    echo "SKIP: ipv6 literal (no ::1 loopback on this host)"
fi
check_says  "redirect loop"           "Too many redirects" \
				      "http://127.0.0.1:$HP/loop"
check_says  "3xx without Location"    "without a usable Location" \
				      "http://127.0.0.1:$HP/noloc"
check_says  "empty host"              "empty host" "http:///x.png"

# TLS behavior depends on whether this vp was built with OpenSSL
run_vp 6 "https://127.0.0.1:$SP/test1.ppm"
if grep -q "built without https support" "$TMP/out"; then
    echo "(no-TLS build: checking refusal messages)"
    check_says "https unsupported"    "built without https support" \
				      "https://127.0.0.1:$SP/test1.ppm"
    check_says "https redirect stop"  "built without https support" \
				      "http://127.0.0.1:$HP/tls"
else
    echo "(TLS build: checking certificate handling)"
    check_says "self-signed rejected" "certificate verification failed" \
				      "https://127.0.0.1:$SP/test1.ppm"
    check_says_shows "-k accepts, warns" "accepting untrusted certificate" \
				      -k "https://127.0.0.1:$SP/test1.ppm"
    check_says "https redirect, no -k" "certificate verification failed" \
				      "http://127.0.0.1:$HP/tls"
    check_shows "https redirect, -k"  -k "http://127.0.0.1:$HP/tls"
    check_says_shows "downgrade warned" "downgrades https to plain http" \
				      -k "https://127.0.0.1:$SP/down"
fi

echo "----"
echo "passed $PASS, failed $FAIL"
[ "$FAIL" -eq 0 ]
