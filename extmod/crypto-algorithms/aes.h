/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Paul Sokolovsky
 * Copyright (c) 2018 Yonatan Goldschmidt
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include "py/mpconfig.h"

#if MICROPY_PY_UCRYPTOLIB

// values follow PEP 272
enum {
    UCRYPTOLIB_MODE_ECB = 1,
    UCRYPTOLIB_MODE_CBC = 2,
    UCRYPTOLIB_MODE_CTR = 6,
};

typedef struct ctr_params {
    // counter is the IV of the AES context.

    size_t offset; // in encrypted_counter
    // encrypted counter
    uint8_t encrypted_counter[16];
} ctr_params_t;


#if MICROPY_SSL_AXTLS
#include "lib/axtls/crypto/crypto.h"

#define AES_CTX_IMPL AES_CTX
#endif

#if MICROPY_SSL_MBEDTLS
#include <mbedtls/aes.h>

// we can't run mbedtls AES key schedule until we know whether we're used for encrypt or decrypt.
// therefore, we store the key & keysize and on the first call to encrypt/decrypt we override them
// with the mbedtls_aes_context, as they are not longer required. (this is done to save space)
struct mbedtls_aes_ctx_with_key {
    union {
        mbedtls_aes_context mbedtls_ctx;
        struct {
            uint8_t key[32];
            uint8_t keysize;
        } init_data;
    } u;
    unsigned char iv[16];
};
#define AES_CTX_IMPL struct mbedtls_aes_ctx_with_key
#endif

#endif

void aes_initial_set_key_impl(AES_CTX_IMPL *ctx, const uint8_t *key, size_t keysize, const uint8_t iv[16]);
void aes_final_set_key_impl(AES_CTX_IMPL *ctx, bool encrypt);
void aes_process_ecb_impl(AES_CTX_IMPL *ctx, const uint8_t in[16], uint8_t out[16], bool encrypt);
void aes_process_cbc_impl(AES_CTX_IMPL *ctx, const uint8_t *in, uint8_t *out, size_t in_len, bool encrypt);
#if MICROPY_PY_UCRYPTOLIB_CTR
STATIC void aes_process_ctr_impl(AES_CTX_IMPL *ctx, const uint8_t *in, uint8_t *out, size_t in_len, ctr_params_t *ctr_params);
#endif