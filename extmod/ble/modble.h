/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Glenn Ruben Bakke
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

#ifndef BLE_H__
#define BLE_H__

/* Examples:

Advertisment:

from ble import Peripheral
p = Peripheral()
p.advertise(device_name="MicroPython")

DB setup:

from ble import Service, Characteristic, UUID, Peripheral, constants
from board import LED

def event_handler(id, handle, data):
    print("BLE event:", id, "handle:", handle)
    print(data)

    if id == constants.EVT_GAP_CONNECTED:
        # connected
        LED(2).on()
    elif id == constants.EVT_GAP_DISCONNECTED:
        # disconnect
        LED(2).off()
    elif id == 80:
        print("id 80, data:", data)

# u0 = UUID("0x180D") # HRM service
# u1 = UUID("0x2A37") # HRM measurement

u0 = UUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
u1 = UUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
u2 = UUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
s = Service(u0)
c0 = Characteristic(u1, props = Characteristic.PROP_WRITE | Characteristic.PROP_WRITE_WO_RESP)
c1 = Characteristic(u2, props = Characteristic.PROP_NOTIFY, attrs = Characteristic.ATTR_CCCD)
s.addCharacteristic(c0)
s.addCharacteristic(c1)
p = Peripheral()
p.addService(s)
p.setConnectionHandler(event_handler)
p.advertise(device_name="micr", services=[s])

*/

#include "py/obj.h"
#define HEX2_FMT "%02x"

extern const mp_obj_type_t ble_uuid_type;
extern const mp_obj_type_t ble_service_type;
extern const mp_obj_type_t ble_characteristic_type;
extern const mp_obj_type_t ble_peripheral_type;
extern const mp_obj_type_t ble_scanner_type;
extern const mp_obj_type_t ble_scan_entry_type;
extern const mp_obj_type_t ble_constants_type;
extern const mp_obj_type_t ble_constants_ad_types_type;

typedef enum {
    BLE_UUID_16_BIT = 1,
    BLE_UUID_128_BIT
} ble_uuid_type_t;

typedef enum {
    BLE_SERVICE_PRIMARY = 1,
    BLE_SERVICE_SECONDARY = 2
} ble_service_type_t;

typedef enum {
    BLE_ADDR_TYPE_PUBLIC = 0,
    BLE_ADDR_TYPE_RANDOM_STATIC = 1,
#if 0
    BLE_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE = 2,
    BLE_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE = 3,
#endif
} ble_addr_type_t;

typedef enum {
    BLE_ROLE_PERIPHERAL,
    BLE_ROLE_CENTRAL
} ble_role_type_t;

typedef struct _ble_uuid_obj_t {
    mp_obj_base_t       base;
    ble_uuid_type_t type;
    uint8_t             value[2];
    uint8_t             uuid_vs_idx;
} ble_uuid_obj_t;

typedef struct _ble_peripheral_obj_t {
    mp_obj_base_t       base;
    ble_role_type_t role;
    volatile uint16_t   conn_handle;
    mp_obj_t            delegate;
    mp_obj_t            notif_handler;
    mp_obj_t            conn_handler;
    mp_obj_t            service_list;
} ble_peripheral_obj_t;

typedef struct _ble_service_obj_t {
    mp_obj_base_t              base;
    uint16_t                   handle;
    uint8_t                    type;
    ble_uuid_obj_t       * p_uuid;
    ble_peripheral_obj_t * p_periph;
    mp_obj_t                   char_list;
    uint16_t                   start_handle;
    uint16_t                   end_handle;
} ble_service_obj_t;

typedef struct _ble_characteristic_obj_t {
    mp_obj_base_t           base;
    uint16_t                handle;
    ble_uuid_obj_t    * p_uuid;
    uint16_t                service_handle;
    uint16_t                user_desc_handle;
    uint16_t                cccd_handle;
    uint16_t                sccd_handle;
    uint8_t                 props;
    uint8_t                 attrs;
    ble_service_obj_t * p_service;
    mp_obj_t                value_data;
} ble_characteristic_obj_t;

typedef struct _ble_descriptor_obj_t {
    mp_obj_base_t           base;
    uint16_t                handle;
    ble_uuid_obj_t    * p_uuid;
} ble_descriptor_obj_t;

typedef struct _ble_delegate_obj_t {
    mp_obj_base_t        base;
} ble_delegate_obj_t;

typedef struct _ble_advertise_data_t {
    uint8_t *  p_device_name;
    uint8_t    device_name_len;
    mp_obj_t * p_services;
    uint8_t    num_of_services;
    uint8_t *  p_data;
    uint8_t    data_len;
    bool       connectable;
} ble_advertise_data_t;

typedef struct _ble_scanner_obj_t {
    mp_obj_base_t base;
    mp_obj_t      adv_reports;
} ble_scanner_obj_t;

typedef struct _ble_scan_entry_obj_t {
    mp_obj_base_t base;
    mp_obj_t      addr;
    uint8_t       addr_type;
    bool          connectable;
    int8_t        rssi;
    mp_obj_t      data;
} ble_scan_entry_obj_t;

typedef enum _ble_prop_t {
    BLE_PROP_BROADCAST      = 0x01,
    BLE_PROP_READ           = 0x02,
    BLE_PROP_WRITE_WO_RESP  = 0x04,
    BLE_PROP_WRITE          = 0x08,
    BLE_PROP_NOTIFY         = 0x10,
    BLE_PROP_INDICATE       = 0x20,
    BLE_PROP_AUTH_SIGNED_WR = 0x40,
} ble_prop_t;

typedef enum _ble_attr_t {
    BLE_ATTR_CCCD           = 0x01,
    BLE_ATTR_SCCD           = 0x02,
} ble_attr_t;

#endif // BLE_H__
