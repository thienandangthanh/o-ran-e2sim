/*
 * E2SM-RC v2.0 APER round-trip test (M3 gate).
 *
 * For BOTH E2SM-RC-ControlHeader (controlHeader-Format1) and
 * E2SM-RC-ControlMessage (controlMessage-Format1):
 *   1. allocate + populate a minimal valid struct
 *   2. aper_encode_to_new_buffer  -> byte buffer (print len + hex)
 *   3. asn_decode (ATS_ALIGNED_BASIC_PER) -> expect RC_OK, consumed == len
 *   4. re-encode the decoded struct, assert byte-identical to (2)
 *   5. xer_fprint both for eyeball
 * Exit non-zero on any failure; print "ROUNDTRIP GREEN" on success.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "E2SM-RC-ControlHeader.h"
#include "E2SM-RC-ControlHeader-Format1.h"
#include "E2SM-RC-ControlMessage.h"
#include "E2SM-RC-ControlMessage-Format1.h"
#include "E2SM-RC-ControlMessage-Format1-Item.h"
#include "RANParameter-ValueType.h"
#include "RANParameter-ValueType-Choice-ElementTrue.h"
#include "RANParameter-Value.h"
#include "UEID.h"
#include "UEID-GNB.h"

static int failures = 0;

/* fill a BIT_STRING with `bits` bits (zeroed) */
static void fill_bitstring(BIT_STRING_t *bs, int bits) {
    int bytes = (bits + 7) / 8;
    bs->buf = calloc(bytes, 1);
    bs->size = bytes;
    bs->bits_unused = bytes * 8 - bits;
}

static void fill_octstring(OCTET_STRING_t *os, const unsigned char *d, int n) {
    os->buf = malloc(n);
    memcpy(os->buf, d, n);
    os->size = n;
}

/* Build a minimal valid UEID = gNB-UEID with amf-UE-NGAP-ID + GUAMI. */
static UEID_t *build_ueid(void) {
    UEID_GNB_t *g = calloc(1, sizeof(*g));
    /* amf-UE-NGAP-ID (INTEGER) = 1234 */
    long amf = 1234;
    asn_long2INTEGER(&g->amf_UE_NGAP_ID, amf);
    /* GUAMI: pLMNIdentity(3 octets), AMFRegionID(8b), AMFSetID(10b), AMFPointer(6b) */
    unsigned char plmn[3] = {0x00, 0xf1, 0x10};
    fill_octstring(&g->guami.pLMNIdentity, plmn, 3);
    fill_bitstring(&g->guami.aMFRegionID, 8);
    fill_bitstring(&g->guami.aMFSetID, 10);
    fill_bitstring(&g->guami.aMFPointer, 6);

    UEID_t *u = calloc(1, sizeof(*u));
    u->present = UEID_PR_gNB_UEID;
    u->choice.gNB_UEID = g;
    return u;
}

/* Generic: encode a PDU, decode it, re-encode, assert byte-identical. */
static int roundtrip(const char *name, asn_TYPE_descriptor_t *td, void *st) {
    printf("\n==== %s ====\n", name);

    /* (2) encode */
    asn_encode_to_new_buffer_result_t r1 =
        asn_encode_to_new_buffer(NULL, ATS_ALIGNED_BASIC_PER, td, st);
    if (r1.result.encoded < 0 || r1.buffer == NULL) {
        printf("[%s] FAIL initial encode (encoded=%zd)\n", name, r1.result.encoded);
        failures++; return 1;
    }
    size_t len1 = r1.result.encoded;
    printf("[%s] encoded len=%zu bytes: ", name, len1);
    for (size_t i = 0; i < len1; i++) printf("%02x", ((unsigned char*)r1.buffer)[i]);
    printf("\n");
    printf("[%s] XER (encoded struct):\n", name);
    xer_fprint(stdout, td, st);

    /* (3) decode */
    void *dec = NULL;
    asn_dec_rval_t rv = asn_decode(NULL, ATS_ALIGNED_BASIC_PER, td, &dec,
                                   r1.buffer, len1);
    if (rv.code != RC_OK) {
        printf("[%s] FAIL decode code=%d consumed=%zu\n", name, rv.code, rv.consumed);
        failures++; free(r1.buffer); return 1;
    }
    if (rv.consumed != len1) {
        printf("[%s] FAIL consumed=%zu != len=%zu\n", name, rv.consumed, len1);
        failures++; free(r1.buffer); return 1;
    }
    printf("[%s] decode RC_OK consumed=%zu == len=%zu\n", name, rv.consumed, len1);
    printf("[%s] XER (decoded struct):\n", name);
    xer_fprint(stdout, td, dec);

    /* (4) re-encode decoded, assert byte-identical */
    printf("[%s] re-encoding decoded struct...\n", name); fflush(stdout);
    asn_encode_to_new_buffer_result_t r2 =
        asn_encode_to_new_buffer(NULL, ATS_ALIGNED_BASIC_PER, td, dec);
    fflush(stdout);
    if (r2.result.encoded < 0 || r2.buffer == NULL) {
        printf("[%s] FAIL re-encode\n", name);
        failures++; free(r1.buffer); ASN_STRUCT_FREE(*td, dec); return 1;
    }
    size_t len2 = r2.result.encoded;
    if (len2 != len1 || memcmp(r1.buffer, r2.buffer, len1) != 0) {
        printf("[%s] FAIL round-trip mismatch len1=%zu len2=%zu\n", name, len1, len2);
        failures++;
    } else {
        printf("[%s] round-trip byte-identical (%zu bytes) OK\n", name, len1);
    }

    free(r1.buffer); free(r2.buffer);
    ASN_STRUCT_FREE(*td, dec);
    return 0;
}

static void test_control_header(void) {
    E2SM_RC_ControlHeader_Format1_t *f1 = calloc(1, sizeof(*f1));
    UEID_t *u = build_ueid();
    f1->ueID = *u;   /* move: copy CHOICE shell (present + inner pointer) */
    free(u);         /* free only the shell; inner gNB_UEID is now owned by f1 */
    f1->ric_Style_Type = 3;
    f1->ric_ControlAction_ID = 1;
    /* ric-ControlDecision OPTIONAL -> leave NULL */

    E2SM_RC_ControlHeader_t *hdr = calloc(1, sizeof(*hdr));
    hdr->ric_controlHeader_formats.present =
        E2SM_RC_ControlHeader__ric_controlHeader_formats_PR_controlHeader_Format1;
    hdr->ric_controlHeader_formats.choice.controlHeader_Format1 = f1;

    roundtrip("E2SM-RC-ControlHeader", &asn_DEF_E2SM_RC_ControlHeader, hdr);
    ASN_STRUCT_FREE(asn_DEF_E2SM_RC_ControlHeader, hdr);
}

static void test_control_message(void) {
    /* one RANParameter item: ID=1, valueType = ElementTrue{ value = INT 42 } */
    E2SM_RC_ControlMessage_Format1_Item_t *item = calloc(1, sizeof(*item));
    item->ranParameter_ID = 1;   /* RANParameter-ID is a NativeInteger (long) */
    item->ranParameter_valueType.present =
        RANParameter_ValueType_PR_ranP_Choice_ElementTrue;
    RANParameter_ValueType_Choice_ElementTrue_t *et = calloc(1, sizeof(*et));
    et->ranParameter_value.present = RANParameter_Value_PR_valueInt;
    et->ranParameter_value.choice.valueInt = 42;
    item->ranParameter_valueType.choice.ranP_Choice_ElementTrue = et;

    E2SM_RC_ControlMessage_Format1_t *f1 = calloc(1, sizeof(*f1));
    ASN_SEQUENCE_ADD(&f1->ranP_List.list, item);

    E2SM_RC_ControlMessage_t *msg = calloc(1, sizeof(*msg));
    msg->ric_controlMessage_formats.present =
        E2SM_RC_ControlMessage__ric_controlMessage_formats_PR_controlMessage_Format1;
    msg->ric_controlMessage_formats.choice.controlMessage_Format1 = f1;

    roundtrip("E2SM-RC-ControlMessage", &asn_DEF_E2SM_RC_ControlMessage, msg);
    ASN_STRUCT_FREE(asn_DEF_E2SM_RC_ControlMessage, msg);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);   /* unbuffered: reliable crash/hang diagnostics */
    test_control_header();
    test_control_message();

    printf("\n========================================\n");
    if (failures == 0) {
        printf("ROUNDTRIP GREEN\n");
        return 0;
    }
    printf("ROUNDTRIP FAILED (%d failure(s))\n", failures);
    return 1;
}
