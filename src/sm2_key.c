/*
 * Copyright (c) 2014 - 2020 The GmSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the GmSSL Project.
 *    (http://gmssl.org/)"
 *
 * 4. The name "GmSSL Project" must not be used to endorse or promote
 *    products derived from this software without prior written
 *    permission. For written permission, please contact
 *    guanzhi1980@gmail.com.
 *
 * 5. Products derived from this software may not be called "GmSSL"
 *    nor may "GmSSL" appear in their names without prior written
 *    permission of the GmSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the GmSSL Project
 *    (http://gmssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE GmSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE GmSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <gmssl/sm2.h>
#include <gmssl/oid.h>
#include <gmssl/asn1.h>
#include <gmssl/pem.h>
#include <gmssl/sm4.h>
#include <gmssl/rand.h>
#include <gmssl/pbkdf2.h>
#include <gmssl/pkcs8.h>
#include <gmssl/error.h>
#include <gmssl/ec.h>
#include <gmssl/x509_alg.h>


int sm2_key_generate(SM2_KEY *key)
{
	SM2_BN x;
	SM2_BN y;
	SM2_JACOBIAN_POINT _P, *P = &_P;

	if (!key) {
		error_print();
		return -1;
	}
	memset(key, 0, sizeof(SM2_KEY));

	do {
		sm2_bn_rand_range(x, SM2_N);
	} while (sm2_bn_is_zero(x));
	sm2_bn_to_bytes(x, key->private_key);

	sm2_jacobian_point_mul_generator(P, x);
	sm2_jacobian_point_get_xy(P, x, y);
	sm2_bn_to_bytes(x, key->public_key.x);
	sm2_bn_to_bytes(y, key->public_key.y);
	return 1;
}

int sm2_key_set_private_key(SM2_KEY *key, const uint8_t private_key[32])
{
	memcpy(&key->private_key, private_key, 32);
	// FIXEM：检查私钥是否在有效的范围内					

	if (sm2_point_mul_generator(&key->public_key, private_key) != 1) {
		error_print();
		return -1;
	}


	return 1;
}

int sm2_key_set_public_key(SM2_KEY *key, const SM2_POINT *public_key)
{
	if (!key || !public_key) {
		error_print();
		return -1;
	}
	memset(key, 0, sizeof(SM2_KEY));
	key->public_key = *public_key;
	return 1;
}

int sm2_key_print(FILE *fp, int fmt, int ind, const char *label, const SM2_KEY *key)
{
	format_print(fp, fmt, ind, "%s\n", label);
	ind += 4;
	sm2_public_key_print(fp, fmt, ind, "publicKey", key);
	format_bytes(fp, fmt, ind, "privateKey", key->private_key, 32);
	return 1;
}

int sm2_public_key_to_der(const SM2_KEY *key, uint8_t **out, size_t *outlen)
{
	uint8_t buf[65];
	size_t len = 0;

	if (!key) {
		return 0;
	}
	sm2_point_to_uncompressed_octets(&key->public_key, buf);
	if (asn1_bit_octets_to_der(buf, sizeof(buf), out, outlen) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_public_key_from_der(SM2_KEY *key, const uint8_t **in, size_t *inlen)
{
	int ret;
	const uint8_t *d;
	size_t dlen;
	SM2_POINT P;

	if ((ret = asn1_bit_octets_from_der(&d, &dlen, in, inlen)) != 1) {
		if (ret < 0) error_print();
		return ret;
	}
	if (dlen != 65) {
		error_print();
		return -1;
	}
	if (sm2_point_from_octets(&P, d, dlen) != 1
		|| sm2_key_set_public_key(key, &P) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_public_key_print(FILE *fp, int fmt, int ind, const char *label, const SM2_KEY *pub_key)
{
	return sm2_point_print(fp, fmt, ind, label, &pub_key->public_key);
}

int sm2_public_key_algor_to_der(uint8_t **out, size_t *outlen)
{
	if (x509_public_key_algor_to_der(OID_ec_public_key, OID_sm2, out, outlen) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_public_key_algor_from_der(const uint8_t **in, size_t *inlen)
{
	int ret;
	int oid;
	int curve;

	if ((ret = x509_public_key_algor_from_der(&oid, &curve, in, inlen)) != 1) {
		if (ret < 0) error_print();
		return ret;
	}
	if (oid != OID_ec_public_key) {
		printf("%s %d: oid = %d\n", __FILE__, __LINE__, oid);
		error_print();
		return -1;
	}
	if (curve != OID_sm2) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_private_key_to_der(const SM2_KEY *key, uint8_t **out, size_t *outlen)
{
	size_t len = 0;
	uint8_t params[64];
	uint8_t pubkey[128];
	uint8_t *params_ptr = params;
	uint8_t *pubkey_ptr = pubkey;
	size_t params_len = 0;
	size_t pubkey_len = 0;

	if (ec_named_curve_to_der(OID_sm2, &params_ptr, &params_len) != 1
		|| sm2_public_key_to_der(key, &pubkey_ptr, &pubkey_len) != 1) {
		error_print();
		return -1;
	}
	if (asn1_int_to_der(EC_private_key_version, NULL, &len) != 1
		|| asn1_octet_string_to_der(key->private_key, 32, NULL, &len) != 1
		|| asn1_explicit_to_der(0, params, params_len, NULL, &len) != 1
		|| asn1_explicit_to_der(1, pubkey, pubkey_len, NULL, &len) != 1
		|| asn1_sequence_header_to_der(len, out, outlen) != 1
		|| asn1_int_to_der(EC_private_key_version, out, outlen) != 1
		|| asn1_octet_string_to_der(key->private_key, 32, out, outlen) != 1
		|| asn1_explicit_to_der(0, params, params_len, out, outlen) != 1
		|| asn1_explicit_to_der(1, pubkey, pubkey_len, out, outlen) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_private_key_from_der(SM2_KEY *key, const uint8_t **in, size_t *inlen)
{
	int ret;
	const uint8_t *d;
	size_t dlen;
	int ver;
	const uint8_t *prikey;
	const uint8_t *params;
	const uint8_t *pubkey;
	size_t prikey_len, params_len, pubkey_len;

	if ((ret = asn1_sequence_from_der(&d, &dlen, in, inlen)) != 1) {
		if (ret < 0) error_print();
		return ret;
	}
	if (asn1_int_from_der(&ver, &d, &dlen) != 1
		|| asn1_octet_string_from_der(&prikey, &prikey_len, &d, &dlen) != 1
		|| asn1_explicit_from_der(0, &params, &params_len, &d, &dlen) != 1
		|| asn1_explicit_from_der(1, &pubkey, &pubkey_len, &d, &dlen) != 1
		|| asn1_check(ver == EC_private_key_version) != 1
		|| asn1_length_is_zero(dlen) != 1) {
		error_print();
		return -1;
	}
	if (params) {
		int curve;
		if (ec_named_curve_from_der(&curve, &params, &params_len) != 1
			|| asn1_check(curve == OID_sm2) != 1
			|| asn1_length_is_zero(params_len) != 1) {
			error_print();
			return -1;
		}
	}
	if (asn1_check(prikey_len == 32) != 1
		|| sm2_key_set_private_key(key, prikey) != 1) {
		error_print();
		return -1;
	}
	// 这里的逻辑上应该是用一个新的公钥来接收公钥，并且判断这个和私钥是否一致
	if (pubkey) {
		SM2_KEY tmp_key;
		if (sm2_public_key_from_der(&tmp_key, &pubkey, &pubkey_len) != 1
			|| asn1_length_is_zero(pubkey_len) != 1) {
			error_print();
			return -1;
		}
		if (sm2_public_key_equ(key, &tmp_key) != 1) {
			error_print();
			return -1;
		}
	}
	return 1;
}

int sm2_private_key_print(FILE *fp, int fmt, int ind, const char *label, const uint8_t *d, size_t dlen)
{
	return ec_private_key_print(fp, fmt, ind, label, d, dlen);
}


#define SM2_PRIVATE_KEY_MAX_SIZE 512 // 需要测试这个buffer的最大值

int sm2_private_key_info_to_der(const SM2_KEY *sm2_key, uint8_t **out, size_t *outlen)
{
	size_t len = 0;
	uint8_t prikey[SM2_PRIVATE_KEY_MAX_SIZE];
	uint8_t *p = prikey;
	size_t prikey_len = 0;

	if (sm2_private_key_to_der(sm2_key, &p, &prikey_len) != 1) {
		error_print();
		return -1;
	}
	if (asn1_int_to_der(PKCS8_private_key_info_version, NULL, &len) != 1
		|| sm2_public_key_algor_to_der(NULL, &len) != 1
		|| asn1_octet_string_to_der(prikey, prikey_len, NULL, &len) != 1
		|| asn1_sequence_header_to_der(len, out, outlen) != 1
		|| asn1_int_to_der(PKCS8_private_key_info_version, out, outlen) != 1
		|| sm2_public_key_algor_to_der(out, outlen) != 1
		|| asn1_octet_string_to_der(prikey, prikey_len, out, outlen) != 1) {
		memset(prikey, 0, sizeof(prikey));
		error_print();
		return -1;
	}
	memset(prikey, 0, sizeof(prikey));
	return 1;
}

int sm2_private_key_info_from_der(SM2_KEY *sm2_key, const uint8_t **attrs, size_t *attrslen,
	const uint8_t **in, size_t *inlen)
{
	int ret;
	const uint8_t *d;
	size_t dlen;
	int version;
	const uint8_t *prikey;
	size_t prikey_len;

	if ((ret = asn1_sequence_from_der(&d, &dlen, in, inlen)) != 1) {
		if (ret < 0) error_print();
		return ret;
	}
	if (asn1_int_from_der(&version, &d, &dlen) != 1
		|| sm2_public_key_algor_from_der(&d, &dlen) != 1
		|| asn1_octet_string_from_der(&prikey, &prikey_len, &d, &dlen) != 1
		|| asn1_implicit_set_from_der(0, attrs, attrslen, &d, &dlen) < 0
		|| asn1_length_is_zero(dlen) != 1) {
		error_print();
		return -1;
	}
	if (asn1_check(version == PKCS8_private_key_info_version) != 1
		|| sm2_private_key_from_der(sm2_key, &prikey, &prikey_len) != 1
		|| asn1_length_is_zero(prikey_len) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_private_key_info_print(FILE *fp, int fmt, int ind, const char *label, const uint8_t *d, size_t dlen)
{
	int ret;
	const uint8_t *p;
	size_t len;
	int val;
	const uint8_t *prikey;
	size_t prikey_len;

	format_print(fp, fmt, ind, "%s\n", label);
	ind += 4;

	if (asn1_int_from_der(&val, &d, &dlen) != 1) goto err;
	format_print(fp, fmt, ind, "version: %d\n", val);
	if (asn1_sequence_from_der(&p, &len, &d, &dlen) != 1) goto err;
	x509_public_key_algor_print(fp, fmt, ind, "privateKeyAlgorithm", p, len);
	if (asn1_octet_string_from_der(&p, &len, &d, &dlen) != 1) goto err;
	if (asn1_sequence_from_der(&prikey, &prikey_len, &p, &len) != 1) goto err;
	ec_private_key_print(fp, fmt, ind + 4, "privateKey", prikey, prikey_len);
	if (asn1_length_is_zero(len) != 1) goto err;
	if ((ret = asn1_implicit_set_from_der(0, &p, &len, &d, &dlen)) < 0) goto err;
	else if (ret) format_bytes(fp, fmt, ind, "attributes", p, len);
	if (asn1_length_is_zero(dlen) != 1) goto err;
	return 1;
err:
	error_print();
	return -1;
}

/*
#define SM2_PRIVATE_KEY_INFO_MAX_SIZE 512 // 计算长度

int sm2_private_key_info_to_pem(const SM2_KEY *key, FILE *fp)
{
	uint8_t buf[SM2_PRIVATE_KEY_INFO_MAX_SIZE];
	uint8_t *p = buf;
	size_t len = 0;

	if (sm2_private_key_info_to_der(key, &p, &len) != 1
		|| pem_write(fp, "PRIVATE KEY", buf, len) != 1) {
		memset(buf, 0, sizeof(buf));
		error_print();
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	return 1;
}

int sm2_private_key_info_from_pem(SM2_KEY *sm2_key, const uint8_t **attrs, size_t *attrslen, FILE *fp)
{
	uint8_t buf[512]; // 这个可能是不够用的，因为attributes可能很长
	const uint8_t *cp = buf;
	size_t len;

	if (pem_read(fp, "PRIVATE KEY", buf, &len, sizeof(buf)) != 1
		|| sm2_private_key_info_from_der(sm2_key, attrs, attrslen, &cp, &len) != 1
		|| asn1_length_is_zero(len) != 1) {
		error_print();
		return -1;
	}
	return 1;
}
*/

int sm2_public_key_info_to_der(const SM2_KEY *pub_key, uint8_t **out, size_t *outlen)
{
	size_t len = 0;
	if (sm2_public_key_algor_to_der(NULL, &len) != 1
		|| sm2_public_key_to_der(pub_key, NULL, &len) != 1
		|| asn1_sequence_header_to_der(len, out, outlen) != 1
		|| sm2_public_key_algor_to_der(out, outlen) != 1
		|| sm2_public_key_to_der(pub_key, out, outlen) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_public_key_info_from_der(SM2_KEY *pub_key, const uint8_t **in, size_t *inlen)
{
	int ret;
	const uint8_t *d;
	size_t dlen;

	if ((ret = asn1_sequence_from_der(&d, &dlen, in, inlen)) != 1) {
		if (ret < 0) error_print();
		return ret;
	}
	if (sm2_public_key_algor_from_der(&d, &dlen) != 1
		|| sm2_public_key_from_der(pub_key, &d, &dlen) != 1
		|| asn1_length_is_zero(dlen) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

/*
int sm2_private_key_to_pem(const SM2_KEY *a, FILE *fp)
{
	uint8_t buf[512];
	uint8_t *p = buf;
	size_t len = 0;

	if (sm2_private_key_to_der(a, &p, &len) != 1) {
		error_print();
		return -1;
	}
	if (pem_write(fp, "EC PRIVATE KEY", buf, len) <= 0) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_private_key_from_pem(SM2_KEY *a, FILE *fp)
{
	uint8_t buf[512];
	const uint8_t *cp = buf;
	size_t len;

	if (pem_read(fp, "EC PRIVATE KEY", buf, &len, sizeof(buf)) != 1) {
		error_print();
		return -1;
	}
	if (sm2_private_key_from_der(a, &cp, &len) != 1
		|| len > 0) {
		error_print();
		return -1;
	}
	return 1;
}
*/

int sm2_public_key_info_to_pem(const SM2_KEY *a, FILE *fp)
{
	uint8_t buf[512];
	uint8_t *p = buf;
	size_t len = 0;

	if (sm2_public_key_info_to_der(a, &p, &len) != 1) {
		error_print();
		return -1;
	}
	if (pem_write(fp, "PUBLIC KEY", buf, len) <= 0) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_public_key_info_from_pem(SM2_KEY *a, FILE *fp)
{
	uint8_t buf[512];
	const uint8_t *cp = buf;
	size_t len;

	if (pem_read(fp, "PUBLIC KEY", buf, &len, sizeof(buf)) != 1) {
		error_print();
		return -1;
	}
	if (sm2_public_key_info_from_der(a, &cp, &len) != 1
		|| len > 0) {
		return -1;
	}
	return 1;
}

int sm2_public_key_equ(const SM2_KEY *sm2_key, const SM2_KEY *pub_key)
{
	if (memcmp(sm2_key, pub_key, sizeof(SM2_POINT)) == 0) {
		return 1;
	}
	return 0;
}

int sm2_public_key_copy(SM2_KEY *sm2_key, const SM2_KEY *pub_key)
{
	return sm2_key_set_public_key(sm2_key, &pub_key->public_key);
}

int sm2_public_key_digest(const SM2_KEY *sm2_key, uint8_t dgst[32])
{
	uint8_t bits[65];
	sm2_point_to_uncompressed_octets(&sm2_key->public_key, bits);
	sm3_digest(bits, sizeof(bits), dgst);
	return 1;
}

int sm2_private_key_info_encrypt_to_der(const SM2_KEY *sm2_key, const char *pass,
	uint8_t **out, size_t *outlen)
{
	int ret = -1;
	uint8_t pkey_info[2560];
	uint8_t *p = pkey_info;
	size_t pkey_info_len = 0;
	uint8_t salt[16];
	int iter = 65536;
	uint8_t iv[16];
	uint8_t key[16];
	SM4_KEY sm4_key;
	uint8_t enced_pkey_info[5120];
	size_t enced_pkey_info_len;

	if (sm2_private_key_info_to_der(sm2_key, &p, &pkey_info_len) != 1
		|| rand_bytes(salt, sizeof(salt)) != 1
		|| rand_bytes(iv, sizeof(iv)) != 1
		|| pbkdf2_genkey(DIGEST_sm3(), pass, strlen(pass),
			salt, sizeof(salt), iter, sizeof(key), key) != 1) {
		error_print();
		goto end;
	}
	sm4_set_encrypt_key(&sm4_key, key);
	if (sm4_cbc_padding_encrypt(
			&sm4_key, iv, pkey_info, pkey_info_len,
			enced_pkey_info, &enced_pkey_info_len) != 1
		|| pkcs8_enced_private_key_info_to_der(
			salt, sizeof(salt), iter, sizeof(key), OID_hmac_sm3,
			OID_sm4_cbc, iv, sizeof(iv),
			enced_pkey_info, enced_pkey_info_len, out, outlen) != 1) {
		error_print();
		goto end;
	}
	ret = 1;
end:
	memset(pkey_info, 0, sizeof(pkey_info));
	memset(key, 0, sizeof(key));
	memset(&sm4_key, 0, sizeof(sm4_key));
	return ret;
}

int sm2_private_key_info_decrypt_from_der(SM2_KEY *sm2,
	const uint8_t **attrs, size_t *attrs_len,
	const char *pass, const uint8_t **in, size_t *inlen)
{
	int ret = -1;
	const uint8_t *salt;
	size_t saltlen;
	int iter;
	int keylen;
	int prf;
	int cipher;
	const uint8_t *iv;
	size_t ivlen;
	uint8_t key[16];
	SM4_KEY sm4_key;
	const uint8_t *enced_pkey_info;
	size_t enced_pkey_info_len;
	uint8_t pkey_info[256];
	const uint8_t *cp = pkey_info;
	size_t pkey_info_len;

	if (pkcs8_enced_private_key_info_from_der(&salt, &saltlen, &iter, &keylen, &prf,
		&cipher, &iv, &ivlen, &enced_pkey_info, &enced_pkey_info_len, in, inlen) != 1
		|| asn1_check(keylen == -1 || keylen == 16) != 1
		|| asn1_check(prf == - 1 || prf == OID_hmac_sm3) != 1
		|| asn1_check(cipher == OID_sm4_cbc) != 1
		|| asn1_check(ivlen == 16) != 1
		|| asn1_length_le(enced_pkey_info_len, sizeof(pkey_info)) != 1) {
		error_print();
		return -1;
	}
	if (pbkdf2_genkey(DIGEST_sm3(), pass, strlen(pass), salt, saltlen, iter, sizeof(key), key) != 1) {
		error_print();
		goto end;
	}
	sm4_set_decrypt_key(&sm4_key, key);
	if (sm4_cbc_padding_decrypt(&sm4_key, iv, enced_pkey_info, enced_pkey_info_len,
			pkey_info, &pkey_info_len) != 1
		|| sm2_private_key_info_from_der(sm2, attrs, attrs_len, &cp, &pkey_info_len) != 1
		|| asn1_length_is_zero(pkey_info_len) != 1) {
		error_print();

		if (pkey_info_len) {
			format_bytes(stderr, 0, 0, "700", cp, pkey_info_len);
		}


		goto end;
	}
	ret = 1;
end:
	memset(&sm4_key, 0, sizeof(sm4_key));
	memset(key, 0, sizeof(key));
	memset(pkey_info, 0, sizeof(pkey_info));
	return ret;
}

int sm2_private_key_info_encrypt_to_pem(const SM2_KEY *sm2_key, const char *pass, FILE *fp)
{
	uint8_t buf[1024];
	uint8_t *p = buf;
	size_t len = 0;

	if (sm2_private_key_info_encrypt_to_der(sm2_key, pass, &p, &len) != 1) {
		error_print();
		return -1;
	}
	if (pem_write(fp, "ENCRYPTED PRIVATE KEY", buf, len) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int sm2_private_key_info_decrypt_from_pem(SM2_KEY *key, const char *pass, FILE *fp)
{
	uint8_t buf[512];
	const uint8_t *cp = buf;
	size_t len;
	const uint8_t *attrs;
	size_t attrs_len;

	if (pem_read(fp, "ENCRYPTED PRIVATE KEY", buf, &len, sizeof(buf)) != 1
		|| sm2_private_key_info_decrypt_from_der(key, &attrs, &attrs_len, pass, &cp, &len) != 1) {
		error_print();
		return -1;
	}
	if (asn1_length_is_zero(len) != 1) {
		format_bytes(stderr, 0, 0, "", cp, len);
		error_print();
		return -1;
	}
	return 1;
}
