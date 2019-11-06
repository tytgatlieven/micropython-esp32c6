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

#include "aes.h"
#include <string.h>
#include <assert.h>

#if MICROPY_SSL_AES_STM32

void aes_initial_set_key_impl(AES_CTX_IMPL *ctx, const uint8_t *key, size_t keysize, const uint8_t iv[16]) {
    ctx->u.init_data.keysize = keysize;
    memcpy(ctx->u.init_data.key, key, keysize);

    if (NULL != iv) {
        memcpy(ctx->iv, iv, sizeof(ctx->iv));
    }
}

void aes_final_set_key_impl(AES_CTX_IMPL *ctx, bool encrypt) {
    // first, copy key aside
    uint8_t key[32];
    uint8_t keysize = ctx->u.init_data.keysize;
    memcpy(key, ctx->u.init_data.key, keysize);
    // now, override key with the mbedtls context object
    mbedtls_aes_init(&ctx->u.mbedtls_ctx);

    // setkey call will succeed, we've already checked the keysize earlier.
    assert(16 == keysize || 32 == keysize);
    if (encrypt) {
        mbedtls_aes_setkey_enc(&ctx->u.mbedtls_ctx, key, keysize * 8);
    } else {
        mbedtls_aes_setkey_dec(&ctx->u.mbedtls_ctx, key, keysize * 8);
    }
}

void aes_process_ecb_impl(AES_CTX_IMPL *ctx, const uint8_t in[16], uint8_t out[16], bool encrypt) {
    mbedtls_aes_crypt_ecb(&ctx->u.mbedtls_ctx, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, in, out);
}

void aes_process_cbc_impl(AES_CTX_IMPL *ctx, const uint8_t *in, uint8_t *out, size_t in_len, bool encrypt) {
    mbedtls_aes_crypt_cbc(&ctx->u.mbedtls_ctx, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, in_len, ctx->iv, in, out);
}

#if MICROPY_PY_UCRYPTOLIB_CTR
void aes_process_ctr_impl(AES_CTX_IMPL *ctx, const uint8_t *in, uint8_t *out, size_t in_len, ctr_params_t *ctr_params) {
    mbedtls_aes_crypt_ctr(&ctx->u.mbedtls_ctx, in_len, &ctr_params->offset, ctx->iv, ctr_params->encrypted_counter, in, out);
}
#endif

#endif
