#!/bin/bash
# Build + run the E2SM-RC v2.0 APER round-trip test against installed libasn1c_e2.a.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

gcc e2sm-rc-roundtrip.c \
  -D ASN_DISABLE_OER_SUPPORT \
  -I ../../asn1c -I ../../asn1c-rc \
  -L/usr/local/lib \
  -Wl,--start-group -lasn1c_e2 -Wl,--end-group \
  -lm \
  -o /tmp/rc-rt

/tmp/rc-rt
