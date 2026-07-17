// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// v1.5.4 point #2: ECDSA migrated from OpenSSL to libsecp256k1.
//
// Build requirements:
//   * libsecp256k1 built with the recovery module:
//         ./configure --enable-module-recovery --enable-experimental
//     (link with -lsecp256k1)
//   * The two libsecp256k1 "contrib" helpers, compiled into this project:
//         contrib/lax_der_parsing.{h,c}              -> ecdsa_signature_parse_der_lax()
//         contrib/lax_der_privatekey_parsing.{h,c}   -> ec_privkey_import_der(), ec_privkey_export_der()
//     These ship with the library and are MIT-licensed. They are what keep
//     signature acceptance and on-disk private keys byte-compatible with the
//     OpenSSL era. Do NOT hand-reimplement them.
//
// OpenSSL remains a dependency of the project for hashing and RNG; here it is
// used only for RAND_bytes() and OPENSSL_cleanse().

#include <vector>
#include <cstring>          // v1.5.4 #2 fix: memcpy/memset for lax DER privkey import

#include <secp256k1.h>
#include <secp256k1_recovery.h>

// libsecp256k1 contrib helpers (see note above)
#include "lax_der_parsing.h"
#include "lax_der_privatekey_parsing.h"

#include <openssl/rand.h>     // RAND_bytes  (RNG stays on OpenSSL, out of scope for #2)
#include <openssl/crypto.h>   // OPENSSL_cleanse

#include "key.h"

// ---------------------------------------------------------------------------
// Global library context
// ---------------------------------------------------------------------------

static secp256k1_context* secp256k1ctx = NULL;

void ECC_Start()
{
    assert(secp256k1ctx == NULL);

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    assert(ctx != NULL);

    // Pass in a random blinding seed to the secp256k1 context (side-channel
    // hardening for signing). RNG stays on OpenSSL.
    {
        unsigned char seed[32];
        if (RAND_bytes(seed, 32) == 1) {
            int ret = secp256k1_context_randomize(ctx, seed);
            assert(ret);
        }
        OPENSSL_cleanse(seed, sizeof(seed));
    }

    secp256k1ctx = ctx;
}

void ECC_Stop()
{
    secp256k1_context* ctx = secp256k1ctx;
    secp256k1ctx = NULL;
    if (ctx != NULL)
        secp256k1_context_destroy(ctx);
}

// ---------------------------------------------------------------------------
// Low-S / signature element range checks (pure byte logic, unchanged)
// ---------------------------------------------------------------------------

int CompareBigEndian(const unsigned char *c1, size_t c1len, const unsigned char *c2, size_t c2len) {
    while (c1len > c2len) {
        if (*c1)
            return 1;
        c1++;
        c1len--;
    }
    while (c2len > c1len) {
        if (*c2)
            return -1;
        c2++;
        c2len--;
    }
    while (c1len > 0) {
        if (*c1 > *c2)
            return 1;
        if (*c2 > *c1)
            return -1;
        c1++;
        c2++;
        c1len--;
    }
    return 0;
}

// Order of secp256k1's generator minus 1.
const unsigned char vchMaxModOrder[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
    0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40
};

// Half of the order of secp256k1's generator minus 1.
const unsigned char vchMaxModHalfOrder[32] = {
    0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x5D,0x57,0x6E,0x73,0x57,0xA4,0x50,0x1D,
    0xDF,0xE9,0x2F,0x46,0x68,0x1B,0x20,0xA0
};

const unsigned char vchZero[0] = {};

bool CKey::CheckSignatureElement(const unsigned char *vch, int len, bool half) {
    return CompareBigEndian(vch, len, vchZero, 0) > 0 &&
           CompareBigEndian(vch, len, half ? vchMaxModHalfOrder : vchMaxModOrder, 32) <= 0;
}

// ---------------------------------------------------------------------------
// CKey
// ---------------------------------------------------------------------------

bool CKey::ComputePubKey()
{
    if (!fHavePrivate)
        return false;
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(secp256k1ctx, &pubkey, vch))
        return false;
    unsigned char out[65];
    size_t outlen = fCompressedPubKey ? 33 : 65;
    secp256k1_ec_pubkey_serialize(secp256k1ctx, out, &outlen, &pubkey,
                                  fCompressedPubKey ? SECP256K1_EC_COMPRESSED : SECP256K1_EC_UNCOMPRESSED);
    vchPubKey.assign(out, out + outlen);
    return true;
}

void CKey::SetCompressedPubKey()
{
    fCompressedPubKey = true;
    if (fHavePrivate)
        ComputePubKey();
}

void CKey::Reset()
{
    fSet = false;
    fHavePrivate = false;
    fCompressedPubKey = false;
    OPENSSL_cleanse(vch, sizeof(vch));
    vchPubKey.clear();
}

CKey::CKey()
{
    vchPubKey.clear();
    Reset();
}

CKey::CKey(const CKey& b)
{
    fSet = b.fSet;
    fHavePrivate = b.fHavePrivate;
    fCompressedPubKey = b.fCompressedPubKey;
    memcpy(vch, b.vch, sizeof(vch));
    vchPubKey = b.vchPubKey;
}

CKey& CKey::operator=(const CKey& b)
{
    fSet = b.fSet;
    fHavePrivate = b.fHavePrivate;
    fCompressedPubKey = b.fCompressedPubKey;
    memcpy(vch, b.vch, sizeof(vch));
    vchPubKey = b.vchPubKey;
    return (*this);
}

CKey::~CKey()
{
    OPENSSL_cleanse(vch, sizeof(vch));
}

bool CKey::IsNull() const
{
    return !fSet;
}

bool CKey::IsCompressed() const
{
    return fCompressedPubKey;
}

void CKey::MakeNewKey(bool fCompressed)
{
    RandAddSeedPerfmon();
    do {
        if (RAND_bytes(vch, sizeof(vch)) != 1)
            throw key_error("CKey::MakeNewKey() : RAND_bytes failed");
    } while (!secp256k1_ec_seckey_verify(secp256k1ctx, vch));

    fHavePrivate = true;
    fCompressedPubKey = fCompressed;
    if (!ComputePubKey())
        throw key_error("CKey::MakeNewKey() : ComputePubKey failed");
    fSet = true;
}

// v1.5.4 #2 fix: the contrib ec_privkey_import_der() only accepts LONG-form DER
// (the ~279-byte explicit-parameters format Bitcoin Core wrote). ChessCoin builds
// keys with EC_KEY_new_by_curve_name(), which under OpenSSL 3.x serializes as a
// compact NAMED-curve key (~118 bytes) whose SEQUENCE length is SHORT-form. The
// contrib parser rejects short-form (`!(*p & 0x80) -> return 0`), so every legacy
// key failed to load ("CPrivKey corrupt"). This importer accepts both length
// forms and still validates the extracted secret via seckey_verify, so it cannot
// admit a bad key. Layout parsed:  SEQUENCE { INTEGER 1, OCTET STRING <=32, ... }.
static int lax_import_der_privkey_any(const secp256k1_context* ctx, unsigned char* out32,
                                      const unsigned char* der, size_t derlen)
{
    const unsigned char* p = der;
    const unsigned char* end = der + derlen;
    memset(out32, 0, 32);
    if (end - p < 1 || *p != 0x30) return 0;                 // SEQUENCE
    p++;
    if (end - p < 1) return 0;
    if (*p & 0x80) {                                         // long-form length
        int lenb = *p & 0x7f; p++;
        if (lenb < 1 || lenb > 2 || end - p < lenb) return 0;
        p += lenb;                                          // value not needed, skip
    } else {
        p++;                                                // short-form length
    }
    if (end - p < 3 || p[0] != 0x02 || p[1] != 0x01 || p[2] != 0x01) return 0;  // version = 1
    p += 3;
    // private-key OCTET STRING (1..32 bytes, right-aligned into the 32-byte secret)
    if (end - p < 2 || p[0] != 0x04 || p[1] > 0x20 ||
        (size_t)(end - p) < (size_t)(2 + p[1])) return 0;
    memcpy(out32 + 32 - p[1], p + 2, p[1]);
    if (!secp256k1_ec_seckey_verify(ctx, out32)) { memset(out32, 0, 32); return 0; }
    return 1;
}

bool CKey::SetPrivKey(const CPrivKey& vchPrivKey)
{
    // Import a DER-encoded private key (the legacy/openssl wallet format) into
    // the raw 32-byte secret. The compression flag is NOT carried by this
    // import; callers that care (wallet load) call SetPubKey() first, which
    // sets fCompressedPubKey, and we then recompute the public key with it.
    if (vchPrivKey.empty() ||
        !lax_import_der_privkey_any(secp256k1ctx, vch, &vchPrivKey[0], vchPrivKey.size())) {
        Reset();
        return false;
    }
    fHavePrivate = true;
    fSet = true;
    if (!ComputePubKey()) {
        Reset();
        return false;
    }
    return true;
}

bool CKey::SetSecret(const CSecret& vchSecret, bool fCompressed)
{
    if (vchSecret.size() != 32)
        throw key_error("CKey::SetSecret() : secret must be 32 bytes");
    if (!secp256k1_ec_seckey_verify(secp256k1ctx, &vchSecret[0])) {
        Reset();
        return false;
    }
    memcpy(vch, &vchSecret[0], 32);
    fHavePrivate = true;
    // Match the original semantics: once compressed, stays compressed.
    fCompressedPubKey = fCompressed || fCompressedPubKey;
    if (!ComputePubKey()) {
        Reset();
        return false;
    }
    fSet = true;
    return true;
}

CSecret CKey::GetSecret(bool &fCompressed) const
{
    if (!fHavePrivate)
        throw key_error("CKey::GetSecret() : key does not contain a private key");
    CSecret vchRet;
    vchRet.resize(32);
    memcpy(&vchRet[0], vch, 32);
    fCompressed = fCompressedPubKey;
    return vchRet;
}

CPrivKey CKey::GetPrivKey() const
{
    if (!fHavePrivate)
        throw key_error("CKey::GetPrivKey() : key does not contain a private key");
    // Export in the same DER form OpenSSL's i2d_ECPrivateKey produced, so
    // wallet "key" records stay byte-compatible.
    CPrivKey vchPrivKey(279, 0);
    size_t privkeylen = vchPrivKey.size();
    int ret = ec_privkey_export_der(secp256k1ctx, (unsigned char*)&vchPrivKey[0], &privkeylen,
                                    vch, fCompressedPubKey ? 1 : 0);
    assert(ret);
    vchPrivKey.resize(privkeylen);
    return vchPrivKey;
}

bool CKey::SetPubKey(const CPubKey& vchPubKeyIn)
{
    const std::vector<unsigned char> v = vchPubKeyIn.Raw();
    secp256k1_pubkey pubkey;
    if (v.empty() || !secp256k1_ec_pubkey_parse(secp256k1ctx, &pubkey, &v[0], v.size())) {
        Reset();
        return false;
    }
    vchPubKey = v;
    fCompressedPubKey = (v.size() == 33);
    fHavePrivate = false;
    fSet = true;
    return true;
}

CPubKey CKey::GetPubKey() const
{
    return CPubKey(vchPubKey);
}

bool CKey::Sign(uint256 hash, std::vector<unsigned char>& vchSig)
{
    vchSig.clear();
    if (!fHavePrivate)
        return false;

    secp256k1_ecdsa_signature sig;
    // RFC6979 deterministic nonce (default). secp256k1 enforces low-S, matching
    // the explicit low-S normalization the OpenSSL path used to do by hand.
    int ret = secp256k1_ecdsa_sign(secp256k1ctx, &sig, (unsigned char*)&hash, vch, NULL, NULL);
    if (!ret)
        return false;

    unsigned char out[72];
    size_t outlen = sizeof(out);
    secp256k1_ecdsa_signature_serialize_der(secp256k1ctx, out, &outlen, &sig);
    vchSig.assign(out, out + outlen);
    return true;
}

bool CKey::SignCompact(uint256 hash, std::vector<unsigned char>& vchSig)
{
    if (!fHavePrivate)
        return false;

    vchSig.resize(65);
    secp256k1_ecdsa_recoverable_signature rsig;
    int ret = secp256k1_ecdsa_sign_recoverable(secp256k1ctx, &rsig, (unsigned char*)&hash, vch, NULL, NULL);
    if (!ret) {
        vchSig.clear();
        return false;
    }

    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(secp256k1ctx, &vchSig[1], &recid, &rsig);
    vchSig[0] = 27 + recid + (fCompressedPubKey ? 4 : 0);
    return true;
}

bool CKey::SetCompactSignature(uint256 hash, const std::vector<unsigned char>& vchSig)
{
    if (vchSig.size() != 65)
        return false;
    int nV = vchSig[0];
    if (nV < 27 || nV >= 35)
        return false;

    bool fComp = false;
    if (nV >= 31) {
        fComp = true;
        nV -= 4;
    }
    int recid = nV - 27;          // 0..3

    secp256k1_ecdsa_recoverable_signature rsig;
    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(secp256k1ctx, &rsig, &vchSig[1], recid))
        return false;

    secp256k1_pubkey pubkey;
    if (!secp256k1_ecdsa_recover(secp256k1ctx, &pubkey, &rsig, (unsigned char*)&hash))
        return false;

    unsigned char out[65];
    size_t outlen = fComp ? 33 : 65;
    secp256k1_ec_pubkey_serialize(secp256k1ctx, out, &outlen, &pubkey,
                                  fComp ? SECP256K1_EC_COMPRESSED : SECP256K1_EC_UNCOMPRESSED);
    vchPubKey.assign(out, out + outlen);
    fCompressedPubKey = fComp;
    fHavePrivate = false;
    fSet = true;
    return true;
}

bool CKey::Verify(uint256 hash, const std::vector<unsigned char>& vchSig)
{
    if (!fSet || vchPubKey.empty() || vchSig.empty())
        return false;

    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_parse(secp256k1ctx, &pubkey, &vchPubKey[0], vchPubKey.size()))
        return false;

    secp256k1_ecdsa_signature sig;
    // CONSENSUS-CRITICAL: accept the loose DER encodings OpenSSL historically
    // produced/accepted, then normalize away high-S, so that every signature
    // that validated under OpenSSL still validates here (and vice versa).
    if (!ecdsa_signature_parse_der_lax(secp256k1ctx, &sig, &vchSig[0], vchSig.size()))
        return false;
    secp256k1_ecdsa_signature_normalize(secp256k1ctx, &sig, &sig);

    return secp256k1_ecdsa_verify(secp256k1ctx, &sig, (unsigned char*)&hash, &pubkey) == 1;
}

bool CKey::IsValid()
{
    if (!fSet)
        return false;

    if (!fHavePrivate) {
        // Public-key-only object: valid iff the stored pubkey parses.
        if (vchPubKey.empty())
            return false;
        secp256k1_pubkey pubkey;
        return secp256k1_ec_pubkey_parse(secp256k1ctx, &pubkey, &vchPubKey[0], vchPubKey.size()) == 1;
    }

    if (!secp256k1_ec_seckey_verify(secp256k1ctx, vch))
        return false;

    bool fCompr;
    CSecret secret = GetSecret(fCompr);
    CKey key2;
    key2.SetSecret(secret, fCompr);
    return GetPubKey() == key2.GetPubKey();
}

// ---------------------------------------------------------------------------
// Runtime sanity check
// ---------------------------------------------------------------------------

bool ECC_InitSanityCheck()
{
    if (secp256k1ctx == NULL)
        return false;

    // Deterministic test secret (1..32).
    CSecret vchSecret;
    vchSecret.resize(32);
    for (int i = 0; i < 32; i++)
        vchSecret[i] = (unsigned char)(i + 1);

    CKey key;
    if (!key.SetSecret(vchSecret, true))
        return false;
    if (!key.IsValid())
        return false;

    // Fixed message hash.
    uint256 hash;
    {
        unsigned char* p = (unsigned char*)&hash;
        for (int i = 0; i < 32; i++)
            p[i] = (unsigned char)(0xA0 + i);
    }

    std::vector<unsigned char> vchSig;
    if (!key.Sign(hash, vchSig))
        return false;
    if (!key.Verify(hash, vchSig))
        return false;

    // Compact-sign + recover round trip.
    std::vector<unsigned char> vchCompact;
    if (!key.SignCompact(hash, vchCompact))
        return false;
    CKey keyRec;
    if (!keyRec.SetCompactSignature(hash, vchCompact))
        return false;
    if (keyRec.GetPubKey() != key.GetPubKey())
        return false;

    return true;
}
