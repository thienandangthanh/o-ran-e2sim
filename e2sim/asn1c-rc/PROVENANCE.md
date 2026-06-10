# asn1c-rc — Provenance & Build

E2SM-RC v2.0 ASN.1 type descriptors vendored alongside e2sim's existing
KPM/COMMON tree (`../asn1c`). This directory holds ONLY the RC-specific add-set
(types e2sim's committed tree does not already provide); shared common types are
reused from `../asn1c` to avoid duplicate symbols.

## ASN.1 source

- `asn/e2sm-rc-2.0.asn`, `asn/e2sm-common.asn`
- Origin: O-RAN-Testbed-Automation @ `a8e3af9`, path
  `RAN_Intelligent_Controllers/Near-Real-Time-RIC/xApps/rc/e2sm/`
- RC-2.0 `E2SM-RC-ControlHeader-Format1 ::= { ueID UEID, ric-Style-Type
  RIC-Style-Type, ric-ControlAction-ID RIC-ControlAction-ID, ric-ControlDecision
  ENUMERATED{accept,reject} OPTIONAL }`; `E2SM-RC-ControlMessage` carries
  `controlMessage-Format1` (RANParameter list).

## Code generator

- Compiler: **mouse07410/asn1c @ `0983e78a`** (reports as asn1c-0.9.29)
- Invocation (from generated-file header comment):
  ```
  asn1c -S <skeletons> -pdu=auto -fincludes-quoted -fcompound-names \
        -findirect-choice -fno-include-deps -no-gen-example -no-gen-OER
  ```
- Generated into `/tmp/rc-gen-m07` on the VM (391 .c). Runtime-ABI-compatible
  with e2sim's committed asn1c tree.

## Symbol-dedup rationale (why only a subset is vendored here)

The add-set = files in the RC-generated tree whose defined `asn_DEF_<X>` symbol
is NOT already defined in `../asn1c/*.c`.

- **Clash common types** (ENB-ID, GlobalENB-ID, GlobalenGNB-ID, GNB-DU-ID,
  GNB-CU-UP-ID, NGENB-DU-ID, …): e2sim's tree emits MODULE-PREFIXED symbols
  (`asn_DEF_E2SM_COMMON_IEs_ENB_ID`) while the RC-only tree emits PLAIN symbols
  (`asn_DEF_ENB_ID`). Distinct symbols + distinct filenames → no collision.
  The PLAIN variants are RC's actual referents, so they ARE vendored here.
- **Shared common types** (UEID, CGI, NR-CGI, RIC-Style-Type, RRC-MessageID, …):
  identical `asn_DEF_<X>` symbol AND filename in both trees → reused from
  `../asn1c`, NOT vendored here (would be a duplicate symbol).
- **asn1c runtime / codec engine** (aper_*, ber_*, constr_*, INTEGER_*, …, the
  files defining no `asn_DEF_`): already present in `../asn1c`, NOT vendored.

Add-set: **168 .c + 168 .h**. Excluded: 101 shared-symbol type files, 122
runtime files (all already in `../asn1c`).

## Build — libasn1c_e2.a WITH RC

```sh
rm -rf /tmp/asnbuild && mkdir -p /tmp/asnbuild && cd /tmp/asnbuild
gcc -O2 -fPIC -D ASN_DISABLE_OER_SUPPORT \
    -I /home/uc/ns-O-RAN-installation/oran-e2sim/e2sim/asn1c \
    -I /home/uc/ns-O-RAN-installation/oran-e2sim/e2sim/asn1c-rc \
    -c /home/uc/ns-O-RAN-installation/oran-e2sim/e2sim/asn1c/*.c \
       /home/uc/ns-O-RAN-installation/oran-e2sim/e2sim/asn1c-rc/*.c
sudo ar rcs /usr/local/lib/libasn1c_e2.a *.o
sudo cp /home/uc/ns-O-RAN-installation/oran-e2sim/e2sim/asn1c-rc/*.h /usr/local/include/e2sim/
```

Duplicate-symbol guard (MUST be empty):
```sh
nm /usr/local/lib/libasn1c_e2.a 2>/dev/null \
  | awk '$2=="T"||$2=="D"||$2=="R"{print $3}' | sort | uniq -d
```

## Round-trip test

`roundtrip/run.sh` builds + runs `roundtrip/e2sm-rc-roundtrip.c`: APER
encode → decode → re-encode for both E2SM-RC-ControlHeader (Format1) and
E2SM-RC-ControlMessage (Format1). Asserts RC_OK, `consumed == len`, and
byte-identical re-encode. Prints `ROUNDTRIP GREEN` on success.

NOTE: `asn_check_constraints()` on E2SM-RC-ControlMessage infinite-loops in
this asn1c build (recursion through the extensible RANParameter-ValueType
CHOICE → RANParameter-Structure → RANParameter chain). It is a diagnostic-only
call and is NOT used by encode/decode; the test does not call it. Avoid
`asn_check_constraints` on RANParameter-bearing PDUs with this generated tree.
