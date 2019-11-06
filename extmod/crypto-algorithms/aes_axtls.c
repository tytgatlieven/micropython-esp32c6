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

#if MICROPY_SSL_AXTLS
STATIC void aes_initial_set_key_impl(AES_CTX_IMPL *ctx, const uint8_t *key, size_t keysize, const uint8_t iv[16]) {
    assert(16 == keysize || 32 == keysize);
    AES_set_key(ctx, key, iv, (16 == keysize) ? AES_MODE_128 : AES_MODE_256);
}

STATIC void aes_final_set_key_impl(AES_CTX_IMPL *ctx, bool encrypt) {
    if (!encrypt) {
        AES_convert_key(ctx);
    }
}

STATIC void aes_process_ecb_impl(AES_CTX_IMPL *ctx, const uint8_t in[16], uint8_t out[16], bool encrypt) {
    memcpy(out, in, 16);
    // We assume that out (vstr.buf or given output buffer) is uint32_t aligned
    uint32_t *p = (uint32_t*)out;
    // axTLS likes it weird and complicated with byteswaps
    for (int i = 0; i < 4; i++) {
        p[i] = MP_HTOBE32(p[i]);
    }
    if (encrypt) {
        AES_encrypt(ctx, p);
    } else {
        AES_decrypt(ctx, p);
    }
    for (int i = 0; i < 4; i++) {
        p[i] = MP_BE32TOH(p[i]);
    }
}

STATIC void aes_process_cbc_impl(AES_CTX_IMPL *ctx, const uint8_t *in, uint8_t *out, size_t in_len, bool encrypt) {
    if (encrypt) {
        AES_cbc_encrypt(ctx, in, out, in_len);
    } else {
        AES_cbc_decrypt(ctx, in, out, in_len);
    }
}

#if MICROPY_PY_UCRYPTOLIB_CTR
// axTLS doesn't have CTR support out of the box. This implements the counter part using the ECB primitive.
STATIC void aes_process_ctr_impl(AES_CTX_IMPL *ctx, const uint8_t *in, uint8_t *out, size_t in_len, ctr_params_t *ctr_params) {
    size_t n = ctr_params->offset;
    uint8_t *const counter = ctx->iv;

    while (in_len--) {
        if (n == 0) {
            aes_process_ecb_impl(ctx, counter, ctr_params->encrypted_counter, true);

            // increment the 128-bit counter
            for (int i = 15; i >= 0; --i) {
                if (++counter[i] != 0) {
                    break;
                }
            }
        }

        *out++ = *in++ ^ ctr_params->encrypted_counter[n];
        n = (n + 1) & 0xf;
    }

    ctr_params->offset = n;
}
#endif

#endif
