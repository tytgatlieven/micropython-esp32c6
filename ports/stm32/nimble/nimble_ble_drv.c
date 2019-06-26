/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Damien P. George
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

#include <assert.h>

#include "py/runtime.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "systick.h"
#include "pendsv.h"

#if MICROPY_PY_NIMBLE

#include "ble_drv.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "transport/uart/ble_hci_uart.h"
#include "nimble/host/src/ble_hs_hci_priv.h" // for ble_hs_hci_cmd_tx
#include "host/ble_hs.h"
// #include "host/ble_hs_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

/******************************************************************************/
// Misc functions needed by Nimble

#include <stdarg.h>

int sprintf(char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(str, 65535, fmt, ap);
    va_end(ap);
    return ret;
}

// Non-const versions of the nimble structs so we can dynamically create them
struct ble_gatt_chr_def_nc {
    /**
     * Pointer to characteristic UUID; use BLE_UUIDxx_DECLARE macros to declare
     * proper UUID; NULL if there are no more characteristics in the service.
     */
    ble_uuid_t *uuid;

    /**
     * Callback that gets executed when this characteristic is read or
     * written.
     */
    ble_gatt_access_fn *access_cb;

    /** Optional argument for callback. */
    void *arg;

    /**
     * Array of this characteristic's descriptors.  NULL if no descriptors.
     * Do not include CCCD; it gets added automatically if this
     * characteristic's notify or indicate flag is set.
     */
    struct ble_gatt_dsc_def *descriptors;

    /** Specifies the set of permitted operations for this characteristic. */
    ble_gatt_chr_flags flags;

    /** Specifies minimum required key size to access this characteristic. */
    uint8_t min_key_size;

    /**
     * At registration time, this is filled in with the characteristic's value
     * attribute handle.
     */
    uint16_t *val_handle;
};



// TODO deal with root pointers

STATIC mp_obj_t characteristic_handles = mp_const_none;

#if 1
#undef malloc
#undef realloc
#undef free
void *malloc(size_t size) {
    printf("NIMBLE malloc(%u)\n", (uint)size);
    return m_malloc(size);
}
void free(void *ptr) {
    printf("NIMBLE free(%p)\n", ptr);
    return m_free(ptr);
}
void *realloc(void *ptr, size_t size) {
    printf("NIMBLE realloc(%p, %u)\n", ptr, (uint)size);
    return m_realloc(ptr, size);
}
#endif

/******************************************************************************/
// RUN LOOP

static bool run_loop_up = false;
static uint16_t nus_conn_handle;

// Stores {nimble_attr_handle : ble_characteristic_obj_t }
mp_obj_t char_map;
// Handle to top level peripheral object
static ble_peripheral_obj_t * p_peripheral;

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

extern void nimble_uart_process(void);
extern void os_eventq_run_all(void);
extern void os_callout_process(void);


STATIC void nimble_poll(void) {
    if (!run_loop_up) {
        return;
    }

    nimble_uart_process();
    os_callout_process();
    os_eventq_run_all();
}

// Poll nimble every 128ms
#define NIMBLE_TICK(tick) (((tick) & ~(SYSTICK_DISPATCH_NUM_SLOTS - 1) & 0x7f) == 0)

void nimble_poll_wrapper(uint32_t ticks_ms) {
    if (run_loop_up && NIMBLE_TICK(ticks_ms)) {
        pendsv_schedule_dispatch(PENDSV_DISPATCH_NIMBLE, nimble_poll);
    }
}

STATIC ble_uuid_any_t uuid_obj_to_nimble_uuid(ble_uuid_obj_t *u) {
    ble_uuid_any_t nimble_uuid;// = BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);
    switch(u->type) {
        case BLE_UUID_16_BIT:
        nimble_uuid.u16.u.type = BLE_UUID_TYPE_16;
        nimble_uuid.u16.value = (u->value[1] << 8) | u->value[0];
        break;

        case BLE_UUID_128_BIT:
        nimble_uuid.u16.u.type = BLE_UUID_TYPE_128;
        uint8_t *value = nimble_uuid.u128.value;
        memcpy(value, u->value, 16);
        break;

        // default:
        // TODO: print error?
    }
    return nimble_uuid;
}

STATIC ble_uuid_obj_t nimble_uuid_to_uuid_obj(ble_uuid_any_t *u) {
    ble_uuid_obj_t uuid;

    switch(u->u.type) {
        case BLE_UUID_TYPE_16:
        uuid.type = BLE_UUID_16_BIT;
        uuid.value[0] = u->u16.value & 0xFF;
        uuid.value[1] = (u->u16.value >> 8) & 0xFF;
        break;

        case BLE_UUID_TYPE_128:
        uuid.type = BLE_UUID_128_BIT;
        uint8_t *value = uuid.value;
        memcpy(value, u->u128.value, 16);
        break;

        // default:
        // TODO: print error?
    }
    return uuid;
}

STATIC int
gatt_svr_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // uint16_t uuid16;

    // uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    // assert(uuid16 != 0);

    ble_uuid_obj_t uuid = nimble_uuid_to_uuid_obj((ble_uuid_any_t *)ctxt->chr->uuid);
    UNUSED(uuid);

    // switch (uuid16) {
    // case BLE_GAP_CHR_UUID16_DEVICE_NAME:
    //     assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
    //     ctxt->chr_access.data = (void *)bleprph_device_name;
    //     ctxt->chr_access.len = strlen(bleprph_device_name);
    //     break;

    // case BLE_GAP_CHR_UUID16_APPEARANCE:
    //     assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
    //     ctxt->chr_access.data = (void *)&bleprph_appearance;
    //     ctxt->chr_access.len = sizeof bleprph_appearance;
    //     break;

    // case BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG:
    //     assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
    //     ctxt->chr_access.data = (void *)&bleprph_privacy_flag;
    //     ctxt->chr_access.len = sizeof bleprph_privacy_flag;
    //     break;

    // case BLE_GAP_CHR_UUID16_RECONNECT_ADDR:
    //     assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
    //     if (ctxt->chr_access.len != sizeof bleprph_reconnect_addr) {
    //         return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    //     }
    //     memcpy(bleprph_reconnect_addr, ctxt->chr_access.data,
    //            sizeof bleprph_reconnect_addr);
    //     break;

    // case BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS:
    //     assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
    //     ctxt->chr_access.data = (void *)&bleprph_pref_conn_params;
    //     ctxt->chr_access.len = sizeof bleprph_pref_conn_params;
    //     break;

    // default:
    //     assert(0);
    //     break;
    // }

    return 0;
}

/******************************************************************************/
// BINDINGS

extern void ble_app_nus_init(void);
extern int ble_nus_read_char(void);
extern void ble_nus_write(size_t len, const uint8_t *buf);

uint32_t ble_drv_stack_enable(void) {
    int32_t err_code = 0;

    ble_app_nus_init();

    ble_hci_uart_init();

    printf("nimble_port_init\n");
    nimble_port_init();
    ble_hs_sched_start();
    printf("nimble_port_init: done\n");

    run_loop_up = true;

    systick_enable_dispatch(SYSTICK_DISPATCH_NIMBLE, nimble_poll_wrapper);

    err_code = ble_gatts_reset();
    printf("ble_gatts_reset() -> %d\n", (int)err_code);

    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    return err_code;
}

uint8_t ble_drv_stack_enabled(void) {
    return run_loop_up;
}

void ble_drv_stack_disable(void) {
    run_loop_up = false;
    // mp_hal_pin_low(MICROPY_HW_BLE_RESET_GPIO);
}

void ble_drv_address_get(ble_drv_addr_t * p_addr) {
    mp_hal_get_mac(MP_HAL_MAC_BDADDR, p_addr->addr);
    p_addr->addr_type = BLE_ADDR_TYPE_PUBLIC; // TODO fix this?
}

bool ble_drv_advertise_data(ble_advertise_data_t * p_adv_params)
{
    /* TODO use p_adv_params */

    uint8_t own_addr_type;
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    nus_conn_handle = 0;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return false;
    }

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){
        // BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 0; // 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return false;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 128; // 80ms
    adv_params.itvl_max = 240; // 150ms
    adv_params.channel_map = 7;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return false;
    }
    return true;
}

void ble_drv_advertise_stop(void) {
    ble_gap_adv_stop();
}

void ble_drv_finalise(ble_peripheral_obj_t * p_peripheral_obj) {
    if (p_peripheral_obj->initialised) {
        return;
    }
    p_peripheral = p_peripheral_obj;
    char_map = mp_obj_new_dict(0);

    // if (characteristic_handles == mp_const_none) {
    //     characteristic_handles = mp_obj_new_dict(1);
    // }

    // mp_obj_dict_store(
    //     characteristic_handles, 
    //     MP_OBJ_NEW_SMALL_INT(ctxt->svc.handle), 
        
    // );

    // finalise all services
    mp_obj_t * services = NULL;
    mp_uint_t  num_services;
    mp_obj_get_array(p_peripheral_obj->service_list, &num_services, &services);
    for (uint16_t s = 0; s < num_services; s++) {
        ble_service_obj_t * p_service = (ble_service_obj_t *)services[s];

        ble_uuid_any_t svc_uuid = uuid_obj_to_nimble_uuid(p_service->p_uuid);

        mp_obj_t * chars = NULL;
        mp_uint_t num_chars = 0;
        mp_obj_get_array(p_service->char_map, &num_chars, &chars);

        struct ble_gatt_svc_def *service_def = gc_alloc((2 * sizeof(struct ble_gatt_svc_def)) + 
                                                          (num_chars * sizeof(struct ble_gatt_chr_def)), 
                                                           false);
        service_def[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
        service_def[0].uuid = &svc_uuid.u;
        // service_def[0]

        // struct ble_gatt_svc_def service_def[] = {
        //     {
        //         /*** Service: Security test. */
        //         .type = BLE_GATT_SVC_TYPE_PRIMARY,
        //         .uuid = &svc_uuid.u,
        //         .characteristics = struct ble_gatt_chr_def[num_chars]
        //     },
        //     //     .characteristics = (struct ble_gatt_chr_def[]) {
        //     //         {
        //     //             /*** Characteristic: RX, writable */
        //     //             .uuid = &gatt_svr_chr_rx.u,
        //     //             .access_cb = gatt_svr_chr_access_sec_test,
        //     //             .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        //     //         },
        //     //         {
        //     //             /*** Characteristic: TX, notifies */
        //     //             .uuid = &gatt_svr_chr_tx.u,
        //     //             .val_handle = &ble_nus_tx_handle,
        //     //             .access_cb = gatt_svr_chr_access_sec_test,
        //     //             .flags = BLE_GATT_CHR_F_NOTIFY,
        //     //         },
        //     //         {
        //     //             0, /* No more characteristics in this service. */
        //     //         }
        //     //     },
        //     // },
        //     {0,},
        // };
        
        ble_uuid_any_t *char_uuids = gc_alloc((num_chars * sizeof(ble_uuid_any_t)), false);
        
        for (uint16_t c = 0; c < num_chars; c++) {
            ble_characteristic_obj_t * p_char = (ble_characteristic_obj_t *)chars[c];

            uint32_t flags = 0;
            flags += (p_char->props & BLE_PROP_READ) ? BLE_GATT_CHR_F_READ : 0;
            flags += (p_char->props & BLE_PROP_WRITE) ? BLE_GATT_CHR_F_WRITE : 0;
            flags += (p_char->props & BLE_PROP_NOTIFY) ? BLE_GATT_CHR_F_NOTIFY : 0;
            // TODO convert more flags

            struct ble_gatt_chr_def_nc *p_buf_char = (void *)&(service_def[0].characteristics[c]);
            
            char_uuids[c] = uuid_obj_to_nimble_uuid(p_char->p_uuid);
            p_buf_char->uuid = &(char_uuids[c].u);
            p_buf_char->access_cb = gatt_svr_chr_access_gap;
            p_buf_char->flags = flags;
        }


        int rc = ble_gatts_count_cfg(service_def);
        if (rc != 0) {
            gc_free(char_uuids);
            gc_free(service_def);
            // TODO raise
            return;// rc;
        }

        rc = ble_gatts_add_svcs(service_def);
        if (rc != 0) {
            gc_free(char_uuids);
            gc_free(service_def);
            // TODO raise
            return;// rc;
        }
        gc_free(char_uuids);
        gc_free(service_def);
    }

}

bool ble_drv_service_add(ble_service_obj_t * p_service_obj) {

    // int rc;

    // p_service_obj->

    // const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    //     {
    //         /*** Service: Security test. */
    //         .type = BLE_GATT_SVC_TYPE_PRIMARY,
    //         .uuid = &gatt_svr_svc_nus.u,
    //         .characteristics = (struct ble_gatt_chr_def[]) {
    //             {
    //                 /*** Characteristic: RX, writable */
    //                 .uuid = &gatt_svr_chr_rx.u,
    //                 .access_cb = gatt_svr_chr_access_sec_test,
    //                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    //             },
    //             {
    //                 /*** Characteristic: TX, notifies */
    //                 .uuid = &gatt_svr_chr_tx.u,
    //                 .val_handle = &ble_nus_tx_handle,
    //                 .access_cb = gatt_svr_chr_access_sec_test,
    //                 .flags = BLE_GATT_CHR_F_NOTIFY,
    //             },
    //             {
    //                 0, /* No more characteristics in this service. */
    //             }
    //         },
    //     },

    //     {
    //         0, /* No more services. */
    //     },
    // };

    // rc = ble_gatts_count_cfg(gatt_svr_svcs);
    // if (rc != 0) {
    //     return rc;
    // }

    // rc = ble_gatts_add_svcs(gatt_svr_svcs);
    // if (rc != 0) {
    //     return rc;
    // }
    return true;
}


// INTERNAL

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
            phy_conn_changed(event->connect.conn_handle);
#endif
            nus_conn_handle = event->connect.conn_handle;
        }
        MODLOG_DFLT(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
        phy_conn_changed(CONN_HANDLE_INVALID);
#endif

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        /* XXX: assume symmetric phy for now */
        phy_update(event->phy_updated.tx_phy);
        return 0;
#endif
    }

    return 0;
}

#undef MODLOG_DFLT
#define MODLOG_DFLT(level, ...) printf(__VA_ARGS__)

static void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);

        mp_obj_t * services = NULL;
        mp_uint_t  num_services;
        mp_obj_get_array(p_peripheral->service_list, &num_services, &services);
        for (uint16_t s = 0; s < num_services; s++) {
            ble_service_obj_t * p_service = (ble_service_obj_t *)services[s];
            ble_uuid_any_t svc_uuid = uuid_obj_to_nimble_uuid(p_service->p_uuid);
            if (0 == ble_uuid_cmp((ble_uuid_t *)&svc_uuid, ctxt->chr.svc_def->uuid)) {
                mp_obj_t char_obj = mp_obj_dict_get(
                    p_service->char_map, 
                    (mp_obj_base_t)nimble_uuid_to_uuid_obj((ble_uuid_any_t *)ctxt->chr.chr_def->uuid)
                );

                mp_obj_dict_store(
                    char_map, 
                    MP_OBJ_NEW_SMALL_INT(ctxt->chr.def_handle),
                    char_obj
                );
            }
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}


// ORIGINAL

// hci_cmd(ogf, ocf, param[, outbuf])
STATIC mp_obj_t nimble_hci_cmd(size_t n_args, const mp_obj_t *args) {
    int ogf = mp_obj_get_int(args[0]);
    int ocf = mp_obj_get_int(args[1]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);

    uint8_t evt_buf[255];
    uint8_t evt_len;
    int rc = ble_hs_hci_cmd_tx(BLE_HCI_OP(ogf, ocf), bufinfo.buf, bufinfo.len, evt_buf, sizeof(evt_buf), &evt_len);

    if (rc != 0) {
        mp_raise_OSError(-rc);
    }

    if (n_args == 3) {
        return mp_obj_new_bytes(evt_buf, evt_len);
    } else {
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_WRITE);
        if (bufinfo.len < evt_len) {
            mp_raise_ValueError("buf too small");
        }
        memcpy(bufinfo.buf, evt_buf, evt_len);
        return MP_OBJ_NEW_SMALL_INT(evt_len);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(nimble_hci_cmd_obj, 3, 4, nimble_hci_cmd);

STATIC mp_obj_t nimble_nus_read(void) {
    uint8_t buf[16];
    size_t i;
    for (i = 0; i < sizeof(buf); ++i) {
        int c = ble_nus_read_char();
        if (c < 0) {
            break;
        }
        buf[i] = c;
    }
    return mp_obj_new_bytes(buf, i);
}
MP_DEFINE_CONST_FUN_OBJ_0(nimble_nus_read_obj, nimble_nus_read);

STATIC mp_obj_t nimble_nus_write(mp_obj_t buf) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);
    ble_nus_write(bufinfo.len, bufinfo.buf);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(nimble_nus_write_obj, nimble_nus_write);

STATIC const mp_rom_map_elem_t nimble_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_nimble) },
    // { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&nimble_init_obj) },
    // { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&nimble_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_hci_cmd), MP_ROM_PTR(&nimble_hci_cmd_obj) },
    { MP_ROM_QSTR(MP_QSTR_nus_read), MP_ROM_PTR(&nimble_nus_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_nus_write), MP_ROM_PTR(&nimble_nus_write_obj) },
};
STATIC MP_DEFINE_CONST_DICT(nimble_module_globals, nimble_module_globals_table);

const mp_obj_module_t nimble_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&nimble_module_globals,
};

#endif // MICROPY_PY_NIMBLE
