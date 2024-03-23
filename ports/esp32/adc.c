/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Nick Moore
 * Copyright (c) 2021 Jonathan Hogg
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

#include "py/mphal.h"
#include "adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"

#define DEFAULT_VREF 1100

void madc_channel_init_helper(machine_adc_obj_t *self) {
    // adc_cali_handle_t handle = NULL;

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = self->block->unit_id,
        .chan = self->channel_id,
        .atten = self->esp_oneshot_channel_config.atten,
        .bitwidth = self->esp_oneshot_channel_config.bitwidth,
    };
    
    switch (self->esp_oneshot_channel_config.bitwidth) {
        #if CONFIG_IDF_TARGET_ESP32
        case 9:
            self->esp_oneshot_channel_config.bitwidth = ADC_BITWIDTH_9;
            break;
        case 10:
            self->esp_oneshot_channel_config.bitwidth = ADC_BITWIDTH_10;
            break;
        case 11:
            self->esp_oneshot_channel_config.bitwidthh = ADC_BITWIDTH_11;
            break;
        #endif
        #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C6 
        case 12:
            self->esp_oneshot_channel_config.bitwidth = ADC_BITWIDTH_12;
            break;
        #endif
        #if CONFIG_IDF_TARGET_ESP32S2
        case 13:
            self->esp_oneshot_channel_config.bitwidth = ADC_BITWIDTH_13;
            break;
        #endif
        default:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid bits."));
    }
    
    ESP_ERROR_CHECK(adc_oneshot_config_channel(self->block->adc_handle, self->channel_id, &self->esp_oneshot_channel_config));
    
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &self->characteristics[self->esp_oneshot_channel_config.atten]));
}

mp_int_t madcblock_read_helper(machine_adc_obj_t *self) {
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(self->block->adc_handle, self->channel_id, &raw));
    return raw;
}

mp_int_t madcblock_read_uv_helper(machine_adc_obj_t *self) {
    int raw = madcblock_read_helper(self);
    mp_int_t m_uv;
    int uv;
    
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(self->characteristics[self->channel_id], raw, &uv));
    
    m_uv = (mp_int_t)uv;

    return m_uv * 1000;
}
