/*
 * This file is part of the MicroPython project, http://micropython.org/
 */

/**
  ******************************************************************************
  * @file    USB_Device/CDC_Standalone/Src/usbd_desc.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    26-February-2014
  * @brief   This file provides the USBD descriptors and string formating method.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

// need this header just for MP_HAL_UNIQUE_ID_ADDRESS
#include "py/mphal.h"

// need this header for any overrides to the below constants
#include "py/mpconfig.h"

#ifndef USBD_LANGID_STRING
#define USBD_LANGID_STRING            0x409
#endif

#ifndef USBD_MANUFACTURER_STRING
#define USBD_MANUFACTURER_STRING      "MicroPython"
#endif

#ifndef USBD_PRODUCT_HS_STRING
#define USBD_PRODUCT_HS_STRING        "Pyboard Virtual Comm Port in HS Mode"
#endif

#ifndef USBD_PRODUCT_FS_STRING
#define USBD_PRODUCT_FS_STRING        "Pyboard Virtual Comm Port in FS Mode"
#endif

#ifndef USBD_CONFIGURATION_HS_STRING
#define USBD_CONFIGURATION_HS_STRING  "Pyboard Config"
#endif

#ifndef USBD_INTERFACE_HS_STRING
#define USBD_INTERFACE_HS_STRING      "Pyboard Interface"
#endif

#ifndef USBD_CONFIGURATION_FS_STRING
#define USBD_CONFIGURATION_FS_STRING  "Pyboard Config"
#endif

#ifndef USBD_INTERFACE_FS_STRING
#define USBD_INTERFACE_FS_STRING      "Pyboard Interface"
#endif


// #define USBD_OS_MSFT100_STRING        "\x4D\x00\x53\x00\x46\x00\x54\x00\x31\x00\x30\x00\x30\x00"
#define USBD_OS_MSFT100_STRING        "MSFT100\xA0"

mp_obj_t mp_obj_desc_manufacturer_str = MP_ROM_NONE;
mp_obj_t mp_obj_desc_product_fs_str = MP_ROM_NONE;
mp_obj_t mp_obj_desc_config_fs_str = MP_ROM_NONE;
mp_obj_t mp_obj_desc_iface_fs_str = MP_ROM_NONE;
mp_obj_t mp_obj_desc_product_hs_str = MP_ROM_NONE;
mp_obj_t mp_obj_desc_config_hs_str = MP_ROM_NONE;
mp_obj_t mp_obj_desc_iface_hs_str = MP_ROM_NONE;


__ALIGN_BEGIN static const uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

// set the VID, PID and device release number
void USBD_SetVIDPIDRelease(usbd_cdc_msc_hid_state_t *usbd, uint16_t vid, uint16_t pid, uint16_t device_release_num, int cdc_only) {
    uint8_t *dev_desc = &usbd->usbd_device_desc[0];

    dev_desc[0] = USB_LEN_DEV_DESC; // bLength
    dev_desc[1] = USB_DESC_TYPE_DEVICE; // bDescriptorType
    dev_desc[2] = 0x00; // bcdUSB
    dev_desc[3] = 0x02; // bcdUSB
    if (cdc_only) {
        // Make it look like a Communications device if we're only
        // using CDC. Otherwise, windows gets confused when we tell it that
        // its a composite device with only a cdc serial interface.
        dev_desc[4] = 0x02; // bDeviceClass
        dev_desc[5] = 0x00; // bDeviceSubClass
        dev_desc[6] = 0x00; // bDeviceProtocol
    } else {
        // For the other modes, we make this look like a composite device.
        dev_desc[4] = 0xef; // bDeviceClass: Miscellaneous Device Class
        dev_desc[5] = 0x02; // bDeviceSubClass: Common Class
        dev_desc[6] = 0x01; // bDeviceProtocol: Interface Association Descriptor
    }
    dev_desc[7] = USB_MAX_EP0_SIZE; // bMaxPacketSize
    dev_desc[8] = LOBYTE(vid); // idVendor
    dev_desc[9] = HIBYTE(vid); // idVendor
    dev_desc[10] = LOBYTE(pid); // idVendor
    dev_desc[11] = HIBYTE(pid); // idVendor
    dev_desc[12] = LOBYTE(device_release_num); // bcdDevice
    dev_desc[13] = HIBYTE(device_release_num); // bcdDevice
    dev_desc[14] = USBD_IDX_MFC_STR; // Index of manufacturer string
    dev_desc[15] = USBD_IDX_PRODUCT_STR; // Index of product string
    dev_desc[16] = USBD_IDX_SERIAL_STR; // Index of serial number string
    dev_desc[17] = USBD_MAX_NUM_CONFIGURATION; // bNumConfigurations
}

/**
  * @brief  Returns the device descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
STATIC uint8_t *USBD_DeviceDescriptor(USBD_HandleTypeDef *pdev, uint16_t *length) {
    uint8_t *dev_desc = ((usbd_cdc_msc_hid_state_t *)pdev->pClassData)->usbd_device_desc;
    *length = USB_LEN_DEV_DESC;
    return dev_desc;
}

/**
  * @brief  Returns a string descriptor
  * @param  idx: Index of the string descriptor to retrieve
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer, or NULL if idx is invalid
  */
STATIC uint8_t *USBD_StrDescriptor(USBD_HandleTypeDef *pdev, uint8_t idx, uint16_t *length) {
    char str_buf[16];
    const char *str = NULL;

    switch (idx) {
        case USBD_IDX_LANGID_STR:
            *length = sizeof(USBD_LangIDDesc);
            return (uint8_t *)USBD_LangIDDesc; // the data should only be read from this buf

        case USBD_IDX_MFC_STR:
            if (mp_obj_desc_manufacturer_str != MP_ROM_NONE) {
                str = mp_obj_str_get_str(mp_obj_desc_manufacturer_str);
            } else {
                str = USBD_MANUFACTURER_STRING;
            }
            break;

        case USBD_IDX_PRODUCT_STR:
            if (pdev->dev_speed == USBD_SPEED_HIGH) {
                if (mp_obj_desc_product_hs_str != MP_ROM_NONE) {
                    str = mp_obj_str_get_str(mp_obj_desc_product_hs_str);
                } else {
                    str = USBD_PRODUCT_HS_STRING;
                }
            } else {
                if (mp_obj_desc_product_fs_str != MP_ROM_NONE) {
                    str = mp_obj_str_get_str(mp_obj_desc_product_fs_str);
                } else {
                    str = USBD_PRODUCT_FS_STRING;
                }
            }
            break;

        case USBD_IDX_SERIAL_STR: {
            // This document: http://www.usb.org/developers/docs/devclass_docs/usbmassbulk_10.pdf
            // says that the serial number has to be at least 12 digits long and that
            // the last 12 digits need to be unique. It also stipulates that the valid
            // character set is that of upper-case hexadecimal digits.
            //
            // The onboard DFU bootloader produces a 12-digit serial number based on
            // the 96-bit unique ID, so for consistency we go with this algorithm.
            // You can see the serial number if you use: lsusb -v
            //
            // See: https://my.st.com/52d187b7 for the algorithim used.

            uint8_t *id = (uint8_t *)MP_HAL_UNIQUE_ID_ADDRESS;
            snprintf(str_buf, sizeof(str_buf),
                "%02X%02X%02X%02X%02X%02X",
                id[11], id[10] + id[2], id[9], id[8] + id[0], id[7], id[6]);

            str = str_buf;
            break;
        }

        case USBD_IDX_CONFIG_STR:
            if (pdev->dev_speed == USBD_SPEED_HIGH) {
                if (mp_obj_desc_config_hs_str != MP_ROM_NONE) {
                    str = mp_obj_str_get_str(mp_obj_desc_config_hs_str);
                } else {
                    str = USBD_CONFIGURATION_HS_STRING;
                }
            } else {
                if (mp_obj_desc_config_fs_str != MP_ROM_NONE) {
                    str = mp_obj_str_get_str(mp_obj_desc_config_fs_str);
                } else {
                    str = USBD_CONFIGURATION_FS_STRING;
                }
            }
            break;

        case USBD_IDX_INTERFACE_STR:
            if (pdev->dev_speed == USBD_SPEED_HIGH) {
                if (mp_obj_desc_iface_hs_str != MP_ROM_NONE) {
                    str = mp_obj_str_get_str(mp_obj_desc_iface_hs_str);
                } else {
                    str = USBD_INTERFACE_HS_STRING;
                }
            } else {
                if (mp_obj_desc_iface_fs_str != MP_ROM_NONE) {
                    str = mp_obj_str_get_str(mp_obj_desc_iface_fs_str);
                } else {
                    str = USBD_INTERFACE_FS_STRING;
                }
            }
            break;

        case 0xEE:
            str = USBD_OS_MSFT100_STRING;
            break;

        default:
            // invalid string index
            return NULL;
    }

    uint8_t *str_desc = ((usbd_cdc_msc_hid_state_t *)pdev->pClassData)->usbd_str_desc;
    USBD_GetString((uint8_t *)str, str_desc, length);
    return str_desc;
}

const USBD_DescriptorsTypeDef USBD_Descriptors = {
    USBD_DeviceDescriptor,
    USBD_StrDescriptor,
};

// #if (USBD_SUPPORT_WINUSB==1)

#define USB_LEN_OS_FEATURE_DESC 0x28
#if defined ( __ICCARM__ ) /* IAR Compiler */
  #pragma data_alignment=4
#endif /* defined ( __ICCARM__ ) */

__ALIGN_BEGIN uint8_t USBD_WINUSB_OSFeatureDesc[USB_LEN_OS_FEATURE_DESC] __ALIGN_END =
{
   0x28, 0, 0, 0, // length
   0, 1,          // bcd version 1.0
   4, 0,          // windex: extended compat ID descritor
   1,             // no of function
   0, 0, 0, 0, 0, 0, 0, // reserve 7 bytes
// function
   0,             // interface no
   0,             // reserved
   'W', 'I', 'N', 'U', 'S', 'B', 0, 0, //  first ID
     0,   0,   0,   0,   0,   0, 0, 0,  // second ID
     0,   0,   0,   0,   0,   0 // reserved 6 bytes      
};
#define USB_LEN_OS_PROPERTY_DESC 0x8E
#if defined ( __ICCARM__ ) /* IAR Compiler */
  #pragma data_alignment=4
#endif /* defined ( __ICCARM__ ) */
__ALIGN_BEGIN uint8_t USBD_WINUSB_OSPropertyDesc[USB_LEN_OS_PROPERTY_DESC] __ALIGN_END =
{
      0x8E, 0, 0, 0,  // length 246 byte
      0x00, 0x01,   // BCD version 1.0
      0x05, 0x00,   // Extended Property Descriptor Index(5)
      0x01, 0x00,   // number of section (1)
//; property section        
      0x84, 0x00, 0x00, 0x00,   // size of property section
      0x1, 0, 0, 0,   //; property data type (1)
      0x28, 0,        //; property name length (42)
      'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 
      'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 
      'a', 0, 'c', 0, 'e', 0, 'G', 0, 'U', 0, 'I', 0, 
      'D', 0,  0, 0,
      // D6805E56-0447-4049-9848-46D6B2AC5D28
      0x4E, 0, 0, 0,  // ; property data length
      '{', 0, '1', 0, '3', 0, 'E', 0, 'B', 0, '3', 0, '6', 0, '0', 0, 
      'B', 0, '-', 0, 'B', 0, 'C', 0, '1', 0, 'E', 0, '-', 0, '4', 0, 
      '6', 0, 'C', 0, 'B', 0, '-', 0, 'A', 0, 'C', 0, '8', 0, 'B', 0, 
      '-', 0, 'E', 0, 'F', 0, '3', 0, 'D', 0, 'A', 0, '4', 0, '7', 0, 
      'B', 0, '4', 0, '0', 0, '6', 0, '2', 0,  '}', 0, 0, 0,
};

// const uint8_t USBD_OS_STRING[8] = { 
//    'M',
//    'S',
//    'F',
//    'T',
//    '1',
//    '0',
//    '0',
//    USB_REQ_MS_VENDOR_CODE, 
// }; 
// uint8_t *USBD_WinUSBOSStrDescriptor(uint16_t *length)
// {
//    USBD_GetString(USBD_OS_MSFT100_STRING, USBD_StrDesc, length);
//    return USBD_StrDesc;
// }
uint8_t *USBD_WinUSBOSFeatureDescriptor(uint16_t *length)
{
  *length = USB_LEN_OS_FEATURE_DESC;
  return USBD_WINUSB_OSFeatureDesc;
}
uint8_t *USBD_WinUSBOSPropertyDescriptor(uint16_t *length)
{
  *length = USB_LEN_OS_PROPERTY_DESC;
   return USBD_WINUSB_OSPropertyDesc;
}
// #endif // (USBD_SUPPORT_WINUSB==1)

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
