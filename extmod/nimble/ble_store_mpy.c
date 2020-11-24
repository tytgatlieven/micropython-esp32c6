/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * This file implements a simple in-RAM key database for BLE host security
 * material and CCCDs.  As this database is only ble_store_ramd in RAM, its
 * contents are lost when the application terminates.
 */

#include <inttypes.h>
#include <string.h>

#include <py/runtime.h>
#include <py/builtin.h>
#include <extmod/vfs.h>
#include <py/stream.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "host/ble_hs.h"

#include "py/obj.h"
#include "py/objstr.h"
#include "py/mpconfig.h"
#include "extmod/modbluetooth.h"



/*****************************************************************************
 * $api                                                                      *
 *****************************************************************************/

/**
 * Searches the database for an object matching the specified criteria.
 *
 * @return                      0 if a key was found; else BLE_HS_ENOENT.
 */
int
ble_store_mpy_read(int obj_type, const union ble_store_key *key, union ble_store_value *value) {

    mp_obj_t rc;

    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
            /* An encryption procedure (bonding) is being attempted.  The nimble
             * stack is asking us to look in our key database for a long-term key
             * corresponding to the specified ediv and random number.
             *
             * Perform a key lookup and populate the context object with the
             * result.  The nimble stack will use this key if this function returns
             * success.
             */        
            rc = mp_bluetooth_bond_read(
                obj_type, key->sec.peer_addr.type, key->sec.peer_addr.val, 
                key->sec.ediv, key->sec.rand_num, key->sec.ediv_rand_present, 0, key->sec.idx
            );
            if (rc == mp_const_none) {
                return BLE_HS_ENOENT;
            }
            if (!mp_obj_is_type(rc, &mp_type_tuple)) {
                mp_raise_TypeError(MP_ERROR_TEXT("Requires tuple with 10 elements"));
            }
            mp_obj_tuple_t *sec_tuple = MP_OBJ_TO_PTR(rc);
            if (sec_tuple->len != 10) {
                mp_raise_TypeError(MP_ERROR_TEXT("Requires tuple with 10 elements"));
            }
            
            value->sec.peer_addr.type = mp_obj_get_int(sec_tuple->items[0]);
            
            if (!mp_obj_is_type(sec_tuple->items[1], &mp_type_bytes)) {
                mp_raise_TypeError(MP_ERROR_TEXT("Element 1 (address) must be bytes"));
            }
            mp_obj_str_t *addr = MP_OBJ_TO_PTR(sec_tuple->items[1]);
            if (addr->len != 6) {
                mp_raise_TypeError(MP_ERROR_TEXT("Element 1 (address) must be 6 bytes long"));
            }
            memcpy(value->sec.peer_addr.val, addr->data, 6);

            value->sec.key_size = mp_obj_get_int(sec_tuple->items[2]);
            value->sec.ediv = mp_obj_get_int(sec_tuple->items[3]);

            if (!mp_obj_is_type(sec_tuple->items[4], &mp_type_bytes)) {
                mp_raise_TypeError(MP_ERROR_TEXT("Element 4 (rand) must be bytes"));
            }
            mp_obj_str_t *rand = MP_OBJ_TO_PTR(sec_tuple->items[4]);
            if (rand->len != 8) {
                mp_raise_TypeError(MP_ERROR_TEXT("Element 4 (rand) must be 8 bytes long"));
            }
            memcpy(&value->sec.rand_num, rand->data, 8);            

            if (sec_tuple->items[5] == mp_const_none) {
                value->sec.ltk_present = false;
            } else {
                value->sec.ltk_present = true;
                if (!mp_obj_is_type(sec_tuple->items[5], &mp_type_bytes)) {
                    mp_raise_TypeError(MP_ERROR_TEXT("Element 5 (ltk) must be None or bytes"));
                }
                mp_obj_str_t *ltk = MP_OBJ_TO_PTR(sec_tuple->items[5]);
                if (ltk->len != 16) {
                    mp_raise_TypeError(MP_ERROR_TEXT("Element 5 (ltk) must be 16 bytes long"));
                }
                memcpy(value->sec.ltk, ltk->data, 16);            
            }

            if (sec_tuple->items[6] == mp_const_none) {
                value->sec.irk_present = false;
            } else {
                value->sec.irk_present = true;
                if (!mp_obj_is_type(sec_tuple->items[6], &mp_type_bytes)) {
                    mp_raise_TypeError(MP_ERROR_TEXT("Element 6 (irk) must be None or bytes"));
                }
                mp_obj_str_t *irk = MP_OBJ_TO_PTR(sec_tuple->items[6]);
                if (irk->len != 16) {
                    mp_raise_TypeError(MP_ERROR_TEXT("Element 6 (irk) must be 16 bytes long"));
                }
                memcpy(value->sec.irk, irk->data, 16);            
            }

            if (sec_tuple->items[7] == mp_const_none) {
                value->sec.csrk_present = false;
            } else {
                value->sec.csrk_present = true;
                if (!mp_obj_is_type(sec_tuple->items[7], &mp_type_bytes)) {
                    mp_raise_TypeError(MP_ERROR_TEXT("Element 7 (csrk) must be None or bytes"));
                }
                mp_obj_str_t *csrk = MP_OBJ_TO_PTR(sec_tuple->items[7]);
                if (csrk->len != 16) {
                    mp_raise_TypeError(MP_ERROR_TEXT("Element 7 (csrk) must be 16 bytes long"));
                }
                memcpy(value->sec.csrk, csrk->data, 16);            
            }

            value->sec.authenticated = mp_obj_get_int(sec_tuple->items[8]);
            value->sec.sc = mp_obj_get_int(sec_tuple->items[9]);

            return 0;

        case BLE_STORE_OBJ_TYPE_CCCD:
            rc = mp_bluetooth_bond_read(
                obj_type, key->cccd.peer_addr.type, key->cccd.peer_addr.val, 
                0, 0, false, key->cccd.chr_val_handle, key->cccd.idx
            );
            if (rc == mp_const_none) {
                return BLE_HS_ENOTSUP;
            }
            if (!mp_obj_is_type(rc, &mp_type_tuple)) {
                mp_raise_TypeError(MP_ERROR_TEXT("Requires tuple with 5 elements"));
            }
            mp_obj_tuple_t *cccd_tuple = MP_OBJ_TO_PTR(rc);
            if (cccd_tuple->len != 5) {
                mp_raise_TypeError(MP_ERROR_TEXT("Requires tuple with 5 elements"));
            }
            
            value->cccd.peer_addr.type = mp_obj_get_int(cccd_tuple->items[0]);
            
            if (!mp_obj_is_type(cccd_tuple->items[1], &mp_type_bytes)) {
                mp_raise_TypeError(MP_ERROR_TEXT("Element 1 (address) must be bytes"));
            }
            mp_obj_str_t *caddr = MP_OBJ_TO_PTR(cccd_tuple->items[1]);
            if (caddr->len != 6) {
                mp_raise_TypeError(MP_ERROR_TEXT("Element 1 (address) must be 6 bytes long"));
            }
            memcpy(value->cccd.peer_addr.val, caddr->data, 6);

            value->cccd.chr_val_handle = mp_obj_get_int(cccd_tuple->items[2]);
            value->cccd.flags = mp_obj_get_int(cccd_tuple->items[3]);
            value->cccd.value_changed = mp_obj_get_int(cccd_tuple->items[4]);
            return 0;
    }

    return BLE_HS_ENOTSUP;
}

/**
 * Adds the specified object to the database.
 *
 * @return                      0 on success; BLE_HS_ESTORE_CAP if the database
 *                                  is full.
 */
int
ble_store_mpy_write(int obj_type, const union ble_store_value *val) {

    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
            mp_bluetooth_bond_write_sec(
                obj_type, val->sec.peer_addr.type, val->sec.peer_addr.val, val->sec.key_size, 
                val->sec.ediv, val->sec.rand_num, val->sec.ltk, val->sec.ltk_present,
                val->sec.irk, val->sec.irk_present, val->sec.csrk, val->sec.csrk_present,
                val->sec.authenticated, val->sec.sc
            );
            break;

        case BLE_STORE_OBJ_TYPE_CCCD:
            mp_bluetooth_bond_write_cccd(
                obj_type, val->cccd.peer_addr.type, val->cccd.peer_addr.val, 
                val->cccd.chr_val_handle, val->cccd.flags, val->cccd.value_changed
            );
            break;

        default:
            return BLE_HS_ENOTSUP;
    }

    return 0;
}

int
ble_store_mpy_delete(int obj_type, const union ble_store_key *key) {
    mp_obj_t rc;
    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
            rc = mp_bluetooth_bond_delete(
                obj_type, key->sec.peer_addr.type, key->sec.peer_addr.val, 
                key->sec.ediv, key->sec.rand_num, key->sec.ediv_rand_present, 0, key->sec.idx
            );
            if (rc == mp_const_none) {
                return BLE_HS_ENOENT;
            }
            break;

        case BLE_STORE_OBJ_TYPE_CCCD:
            rc = mp_bluetooth_bond_delete(
                obj_type, key->cccd.peer_addr.type, key->cccd.peer_addr.val, 
                0, 0, false, key->cccd.chr_val_handle, key->cccd.idx
            );
            if (rc == mp_const_none) {
                return BLE_HS_ENOENT;
            }
            break;

        default:
            return BLE_HS_ENOTSUP;
    }

    return 0;

}

void
ble_store_ram_init(void) {
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    ble_hs_cfg.store_read_cb = ble_store_mpy_read;
    ble_hs_cfg.store_write_cb = ble_store_mpy_write;
    ble_hs_cfg.store_delete_cb = ble_store_mpy_delete;
}

