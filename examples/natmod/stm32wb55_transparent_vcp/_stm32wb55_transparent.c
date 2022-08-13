#include <string.h>
#include "py/dynruntime.h"
#include "py/stream.h"
#include "stm32wbxx_ll_utils.h"
#include "stm32wbxx_ll_system.h"
#include "ports/stm32/mpbthciport.h"
#include "ports/stm32/rfcore.h"

// Based on STM32CubeWB\Projects\P-NUCLEO-WB55.USBDongle\Applications\BLE\BLE_TransparentModeVCP

// Don't enable this if stdio is used for transp comms.
#define DEBUG_printf(...)  mp_printf(&mp_plat_print, "rfcore_transp: " __VA_ARGS__)

// TODO Check ble_init_params vs SHCI_C2_BLE_Init /
// https://github.com/STMicroelectronics/STM32CubeWB/blob/83aacadecbd5136ad3194f39e002ff50a5439ad9/Projects/STM32WB5MM-DK/Applications/BLE/BLE_Mesh_ThermometerSensor/Core/Inc/app_conf.h#L234

#define FW_Version 1

#define STATE_IDLE 0
#define STATE_NEED_LEN 1
#define STATE_IN_PAYLOAD 2

#define HCI_KIND_BT_CMD (0x01) // <kind=1><?><?><len>
#define HCI_KIND_BT_ACL (0x02) // <kind=2><?><?><len LSB><len MSB>
#define HCI_KIND_BT_EVENT (0x04) // <kind=4><op><len><data...>
#define HCI_KIND_VENDOR_RESPONSE (0x11)
#define HCI_KIND_VENDOR_EVENT (0x12)
#define HCI_KIND_LOCAL_CMD (0x20) // Used by CubeMonitor to query the device
#define HCI_KIND_LOCAL_RSP (0x21)

/* Get the OGF and OCF from the opcode in the command */
#define BLE_HCI_OGF(opcode)                 (((opcode) >> 10) & 0x003F)
#define BLE_HCI_OCF(opcode)                 ((opcode) & 0x03FF)

#define PACKED_STRUCT struct __attribute__((packed))

/**
 * Version
 * [0:3]   = Build - 0: Untracked - 15:Released - x: Tracked version
 * [4:7]   = branch - 0: Mass Market - x: ...
 * [8:15]  = Subversion
 * [16:23] = Version minor
 * [24:31] = Version major
 *
 * Memory Size
 * [0:7]   = Flash ( Number of 4k sector)
 * [8:15]  = Reserved ( Shall be set to 0 - may be used as flash extension )
 * [16:23] = SRAM2b ( Number of 1k sector)
 * [24:31] = SRAM2a ( Number of 1k sector)
 */
typedef PACKED_STRUCT {
    uint32_t Version;
}
MB_SafeBootInfoTable_t;

typedef PACKED_STRUCT {
    uint32_t Version;
    uint32_t MemorySize;
    uint32_t FusInfo;
}
MB_FusInfoTable_t;

typedef PACKED_STRUCT {
    uint32_t Version;
    uint32_t MemorySize;
    uint32_t InfoStack;
    uint32_t Reserved;
}
MB_WirelessFwInfoTable_t;

typedef struct {
    MB_SafeBootInfoTable_t SafeBootInfoTable;
    MB_FusInfoTable_t FusInfoTable;
    MB_WirelessFwInfoTable_t WirelessFwInfoTable;
} MB_DeviceInfoTable_t;

typedef struct {
    uint8_t *pcmd_buffer;
    uint8_t *pcs_buffer;
    uint8_t *pevt_queue;
    uint8_t *phci_acl_data_buffer;
} MB_BleTable_t;

typedef struct {
    uint8_t *notack_buffer;
    uint8_t *clicmdrsp_buffer;
    uint8_t *otcmdrsp_buffer;
    uint8_t *clinot_buffer;
} MB_ThreadTable_t;

typedef struct {
    uint8_t *clicmdrsp_buffer;
    uint8_t *m0cmd_buffer;
} MB_LldTestsTable_t;

typedef struct {
    uint8_t *cmdrsp_buffer;
    uint8_t *m0cmd_buffer;
} MB_BleLldTable_t;

typedef struct {
    uint8_t *notifM0toM4_buffer;
    uint8_t *appliCmdM4toM0_buffer;
    uint8_t *requestM0toM4_buffer;
} MB_ZigbeeTable_t;
/**
 * msg
 * [0:7]   = cmd/evt
 * [8:31] = Reserved
 */
typedef struct {
    uint8_t *pcmd_buffer;
    uint8_t *sys_queue;
} MB_SysTable_t;

typedef struct {
    uint8_t *spare_ble_buffer;
    uint8_t *spare_sys_buffer;
    uint8_t *blepool;
    uint32_t blepoolsize;
    uint8_t *pevt_free_buffer_queue;
    uint8_t *traces_evt_pool;
    uint32_t tracespoolsize;
} MB_MemManagerTable_t;

typedef struct {
    uint8_t *traces_queue;
} MB_TracesTable_t;

typedef struct {
    uint8_t *p_cmdrsp_buffer;
    uint8_t *p_notack_buffer;
    uint8_t *evt_queue;
} MB_Mac_802_15_4_t;

typedef struct {
    MB_DeviceInfoTable_t *p_device_info_table;
    MB_BleTable_t *p_ble_table;
    MB_ThreadTable_t *p_thread_table;
    MB_SysTable_t *p_sys_table;
    MB_MemManagerTable_t *p_mem_manager_table;
    MB_TracesTable_t *p_traces_table;
    MB_Mac_802_15_4_t *p_mac_802_15_4_table;
    MB_ZigbeeTable_t *p_zigbee_table;
    MB_LldTestsTable_t *p_lld_tests_table;
    MB_BleLldTable_t *p_ble_lld_table;
} MB_RefTable_t;

typedef enum {
    LHCI_8bits = 1,
    LHCI_16bits = 2,
    LHCI_32bits = 4,
} LHCI_Busw_t;

#define LHCI_OGF (0x3F)
#define LHCI_OCF_BASE (0x160)

#define LHCI_OCF_C1_WRITE_REG (LHCI_OCF_BASE + 0)
#define LHCI_OPCODE_C1_WRITE_REG ((LHCI_OGF << 10) + LHCI_OCF_C1_WRITE_REG)
typedef PACKED_STRUCT {
    LHCI_Busw_t busw;
    uint32_t mask;
    uint32_t add;
    uint32_t val;
}
LHCI_C1_Write_Register_cmd_t;

#define LHCI_OCF_C1_READ_REG (LHCI_OCF_BASE + 1)
#define LHCI_OPCODE_C1_READ_REG ((LHCI_OGF << 10) + LHCI_OCF_C1_READ_REG)
typedef PACKED_STRUCT {
    LHCI_Busw_t busw;
    uint32_t add;
}
LHCI_C1_Read_Register_cmd_t;

typedef PACKED_STRUCT {
    uint8_t status;
    uint32_t val;
}
LHCI_C1_Read_Register_ccrp_t;

#define LHCI_OCF_C1_DEVICE_INF (LHCI_OCF_BASE + 2)
#define LHCI_OPCODE_C1_DEVICE_INF ((LHCI_OGF << 10) + LHCI_OCF_C1_DEVICE_INF)
typedef PACKED_STRUCT {
    uint8_t status;
    uint16_t rev_id;      /* from DBGMCU_ICODE */
    uint16_t dev_code_id; /* from DBGMCU_ICODE */
    uint8_t package_type; /* from package data register */
    uint8_t device_type_id; /* from FLASH UID64 */
    uint32_t st_company_id; /* from FLASH UID64 */
    uint32_t uid64;       /* from FLASH UID64 */
    uint32_t uid96_0;     /* from Unique device ID register */
    uint32_t uid96_1;     /* from Unique device ID register */
    uint32_t uid96_2;     /* from Unique device ID register */
    MB_SafeBootInfoTable_t SafeBootInf;
    MB_FusInfoTable_t FusInf;
    MB_WirelessFwInfoTable_t WirelessFwInf;
    uint32_t AppFwInf;
}
LHCI_C1_Device_Information_ccrp_t;

#define TL_BLECMD_PKT_TYPE (0x01)
#define TL_ACL_DATA_PKT_TYPE (0x02)
#define TL_BLEEVT_PKT_TYPE (0x04)
#define TL_OTCMD_PKT_TYPE (0x08)
#define TL_OTRSP_PKT_TYPE (0x09)
#define TL_CLICMD_PKT_TYPE (0x0A)
#define TL_OTNOT_PKT_TYPE (0x0C)
#define TL_OTACK_PKT_TYPE (0x0D)
#define TL_CLINOT_PKT_TYPE (0x0E)
#define TL_CLIACK_PKT_TYPE (0x0F)
#define TL_SYSCMD_PKT_TYPE (0x10)
#define TL_SYSRSP_PKT_TYPE (0x11)
#define TL_SYSEVT_PKT_TYPE (0x12)
#define TL_CLIRESP_PKT_TYPE (0x15)
#define TL_M0CMD_PKT_TYPE (0x16)
#define TL_LOCCMD_PKT_TYPE (0x20)
#define TL_LOCRSP_PKT_TYPE (0x21)
#define TL_TRACES_APP_PKT_TYPE (0x40)
#define TL_TRACES_WL_PKT_TYPE (0x41)

#define TL_CMD_HDR_SIZE (4)
#define TL_EVT_HDR_SIZE (3)
#define TL_EVT_CS_PAYLOAD_SIZE (4)

#define TL_BLEEVT_CC_OPCODE (0x0E)
#define TL_BLEEVT_CS_OPCODE (0x0F)
#define TL_BLEEVT_VS_OPCODE (0xFF)


typedef PACKED_STRUCT {
    uint16_t cmdcode;
    uint8_t plen;
    uint8_t payload[255];
}
TL_Cmd_t;

typedef PACKED_STRUCT {
    uint8_t type;
    TL_Cmd_t cmd;
}
TL_CmdPacket_t;

typedef PACKED_STRUCT {
    uint8_t numcmd;
    uint16_t cmdcode;
    uint8_t payload[1];
}
TL_CcEvt_t;

typedef PACKED_STRUCT {
    uint8_t evtcode;
    uint8_t plen;
    uint8_t payload[1];
}
TL_Evt_t;

typedef PACKED_STRUCT {
    uint8_t type;
    TL_Evt_t evt;
}
TL_EvtSerial_t;

// Callback python function, can be used to blink LED etc on each comm
mp_obj_t callback_o;

void mpy_cb(bool on) {
    if (callback_o != mp_const_none) {
        mp_call_function_n_kw(callback_o, 1, 0, (mp_obj_t[]) {(on) ? mp_const_true : mp_const_false});
    }
}

// Stream I/O

mp_obj_t stream_in_o;
const mp_stream_p_t *stream_in_p;
mp_obj_t stream_out_o;
const mp_stream_p_t *stream_out_p;

mp_uint_t mpstream_write(const void *buf, size_t len) {
    int errcode;
    mp_uint_t out_sz = stream_out_p->write(stream_out_o, buf, len, &errcode);
    if (out_sz == MP_STREAM_ERROR) {
        return -1;
    }
    return out_sz;
}

mp_uint_t mpstream_read(void *buf, size_t len) {
    int errcode;
    mp_uint_t out_sz = stream_in_p->read(stream_in_o, buf, len, &errcode);
    if (out_sz == MP_STREAM_ERROR) {
        return -1;
    }
    return out_sz;
}

mp_uint_t mpstream_poll(uintptr_t poll_flags) {
    int errcode = 0;
    mp_uint_t ret = 0;
    ret = stream_in_p->ioctl(stream_in_o, MP_STREAM_POLL, poll_flags, &errcode);
    return ret;
}

// CPU1 (local) interaction functions

STATIC void LHCI_C1_Write_Register(TL_CmdPacket_t *pcmd) {
    LHCI_C1_Write_Register_cmd_t *p_param;
    uint32_t primask_bit;

    primask_bit = __get_PRIMASK(); /**< backup PRIMASK bit */

    p_param = (LHCI_C1_Write_Register_cmd_t *)pcmd->cmd.payload;

    switch (p_param->busw) {
        case LHCI_8bits:
            __disable_irq();

            *(uint8_t *)(p_param->add) = ((*(uint8_t *)(p_param->add)) & (~(p_param->mask))) | (p_param->val & p_param->mask);

            __set_PRIMASK(primask_bit); /**< Restore PRIMASK bit*/
            break;

        case LHCI_16bits:
            __disable_irq();

            *(uint16_t *)(p_param->add) = ((*(uint16_t *)(p_param->add)) & (~(p_param->mask))) | (p_param->val & p_param->mask);

            __set_PRIMASK(primask_bit); /**< Restore PRIMASK bit*/
            break;

        default: /**< case SHCI_32BITS */
            __disable_irq();

            *(uint32_t *)(p_param->add) = ((*(uint32_t *)(p_param->add)) & (~(p_param->mask))) | (p_param->val & p_param->mask);

            __set_PRIMASK(primask_bit); /**< Restore PRIMASK bit*/
            break;
    }

    ((TL_EvtSerial_t *)pcmd)->type = TL_LOCRSP_PKT_TYPE;
    ((TL_EvtSerial_t *)pcmd)->evt.evtcode = TL_BLEEVT_CC_OPCODE;
    ((TL_EvtSerial_t *)pcmd)->evt.plen = TL_EVT_CS_PAYLOAD_SIZE;
    ;
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->cmdcode = LHCI_OPCODE_C1_WRITE_REG;
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload[0] = 0x00;
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->numcmd = 1;

    return;
}

STATIC void LHCI_C1_Read_Register(TL_CmdPacket_t *pcmd) {
    LHCI_C1_Read_Register_cmd_t *p_param;
    uint32_t rsp_val;
    uint8_t busw;

    p_param = (LHCI_C1_Read_Register_cmd_t *)pcmd->cmd.payload;
    busw = p_param->busw;

    switch (busw) {
        case LHCI_8bits:
            rsp_val = *(uint8_t *)(p_param->add);
            break;

        case LHCI_16bits:
            rsp_val = *(uint16_t *)(p_param->add);
            break;

        default: /**< case SHCI_32BITS */
            rsp_val = *(uint32_t *)(p_param->add);
            break;
    }

    ((TL_EvtSerial_t *)pcmd)->type = TL_LOCRSP_PKT_TYPE;
    ((TL_EvtSerial_t *)pcmd)->evt.evtcode = TL_BLEEVT_CC_OPCODE;
    ((TL_EvtSerial_t *)pcmd)->evt.plen = TL_EVT_HDR_SIZE + sizeof(LHCI_C1_Read_Register_ccrp_t);
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->cmdcode = LHCI_OPCODE_C1_READ_REG;
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->numcmd = 1;
    ((LHCI_C1_Read_Register_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->status =
        0x00;

    memcpy(
        (void *)&(((LHCI_C1_Read_Register_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->val),
        &rsp_val,
        4);

    return;
}

STATIC void LHCI_C1_Read_Device_Information(TL_CmdPacket_t *pcmd) {
    uint32_t ipccdba;
    MB_RefTable_t *p_ref_table;

    ipccdba = READ_BIT(FLASH->IPCCBR, FLASH_IPCCBR_IPCCDBA);
    p_ref_table = (MB_RefTable_t *)((ipccdba << 2) + SRAM2A_BASE);

    ((TL_EvtSerial_t *)pcmd)->type = TL_LOCRSP_PKT_TYPE;
    ((TL_EvtSerial_t *)pcmd)->evt.evtcode = TL_BLEEVT_CC_OPCODE;
    ((TL_EvtSerial_t *)pcmd)->evt.plen = TL_EVT_HDR_SIZE + sizeof(LHCI_C1_Device_Information_ccrp_t);
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->cmdcode = LHCI_OPCODE_C1_DEVICE_INF;
    ((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->numcmd = 1;

    /**
     * status
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->status =
        0x00;

    /**
     * revision id
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->rev_id =
        (uint16_t)LL_DBGMCU_GetRevisionID();

    /**
     * device code id
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->dev_code_id =
        (uint16_t)LL_DBGMCU_GetDeviceID();

    /**
     * package type
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->package_type =
        (uint8_t)LL_GetPackageType();

    /**
     * device type id
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->device_type_id =
        (uint8_t)LL_FLASH_GetDeviceID();

    /**
     * st company id
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->st_company_id =
        LL_FLASH_GetSTCompanyID();

    /**
     * UID64
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->uid64 =
        LL_FLASH_GetUDN();

    /**
     * UID96_0
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->uid96_0 =
        LL_GetUID_Word0();

    /**
     * UID96_1
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->uid96_1 =
        LL_GetUID_Word1();

    /**
     * UID96_2
     */
    ((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->uid96_2 =
        LL_GetUID_Word2();

    /**
     * SafeBootInf
     */
    memcpy(
        &(((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->SafeBootInf),
        &(p_ref_table->p_device_info_table->SafeBootInfoTable),
        sizeof(MB_SafeBootInfoTable_t));

    /**
     * FusInf
     */
    memcpy(
        &(((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->FusInf),
        &(p_ref_table->p_device_info_table->FusInfoTable),
        sizeof(MB_FusInfoTable_t));

    /**
     * WirelessFwInf
     */
    memcpy(
        &(((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->WirelessFwInf),
        &(p_ref_table->p_device_info_table->WirelessFwInfoTable),
        sizeof(MB_WirelessFwInfoTable_t));

    /**
     * AppFwInf
     */
    (((LHCI_C1_Device_Information_ccrp_t *)(((TL_CcEvt_t *)(((TL_EvtSerial_t *)pcmd)->evt.payload))->payload))->AppFwInf) =
        FW_Version;

    return;
}

STATIC void local_hci_cmd(size_t len, const uint8_t *buffer) {

    TL_CmdPacket_t *SysLocalCmd = (TL_CmdPacket_t *)buffer;
    TL_EvtSerial_t *SysLocalRsp = (TL_EvtSerial_t *)buffer;

    switch (SysLocalCmd->cmd.cmdcode) {
        case LHCI_OPCODE_C1_WRITE_REG:
            LHCI_C1_Write_Register(SysLocalCmd);
            break;

        case LHCI_OPCODE_C1_READ_REG:
            LHCI_C1_Read_Register(SysLocalCmd);
            break;

        case LHCI_OPCODE_C1_DEVICE_INF:
            LHCI_C1_Read_Device_Information(SysLocalCmd);
            break;

        default:
            ((TL_CcEvt_t *)(SysLocalRsp->evt.payload))->cmdcode = SysLocalCmd->cmd.cmdcode;
            ((TL_CcEvt_t *)(SysLocalRsp->evt.payload))->payload[0] = 0x01;
            ((TL_CcEvt_t *)(SysLocalRsp->evt.payload))->numcmd = 1;
            SysLocalRsp->type = TL_LOCRSP_PKT_TYPE;
            SysLocalRsp->evt.evtcode = TL_BLEEVT_CC_OPCODE;
            SysLocalRsp->evt.plen = TL_EVT_CS_PAYLOAD_SIZE;

            break;
    }
    mpstream_write((const char *)buffer, SysLocalRsp->evt.plen + TL_EVT_HDR_SIZE);

    mpy_cb(false);
}


STATIC mp_obj_t rfcore_transparent(size_t n_args, const mp_obj_t *args) {
    mp_obj_t ble = args[0];
    mp_obj_t stream_in = args[1];
    mp_obj_t stream_out = args[2];
    mp_obj_t callback = args[3];
    // Make sure we got a stream object
    stream_in_p = mp_get_stream_raise(stream_in, MP_STREAM_OP_READ | MP_STREAM_OP_IOCTL);
    stream_in_o = stream_in;

    stream_out_p = mp_get_stream_raise(stream_out, MP_STREAM_OP_WRITE);
    stream_out_o = stream_out;

    callback_o = callback;

    // mp_obj_t stm_module = mp_import_name(MP_QSTR_stm, mp_const_none, MP_OBJ_NEW_SMALL_INT(0));
    // mp_obj_t rfcore_ble_hci_obj = mp_import_from(stm_module, MP_QSTR_rfcore_ble_hci);

    uint8_t buf[1024];
    uint8_t rsp[255];
    mp_obj_t rsp_ba = mp_obj_new_bytearray_by_ref(sizeof(rsp), rsp);
    size_t rx = 0;
    size_t len = 0;
    int state = 0;
    int cmd_type = 0;

    while (true) {
        if (state == STATE_IN_PAYLOAD && len == 0) {

            if (cmd_type == HCI_KIND_LOCAL_CMD) {
                // process the command directly
                DEBUG_printf("local_hci_cmd\n");
                local_hci_cmd(rx, buf);
            } else {
                // forward packet to rfcore

                uint16_t opcode = buf[1] | (((uint16_t)buf[2]) << 8);
                DEBUG_printf("rfcore_ble_hci_cmd opcode 0x%x\n", opcode);
                DEBUG_printf("rfcore_ble_hci_cmd len 0x%x\n", buf[3]);
                mp_obj_t hci_cmd[] = {
                    NULL, NULL, // fn, self
                    MP_OBJ_NEW_SMALL_INT(BLE_HCI_OGF(opcode)),
                    MP_OBJ_NEW_SMALL_INT(BLE_HCI_OCF(opcode)),
                    mp_obj_new_bytes(&buf[4], buf[3]), // buf
                    rsp_ba};
                mp_load_method(ble, MP_QSTR_hci_cmd, hci_cmd);
                mp_obj_t status = mp_call_function_n_kw(hci_cmd[0], 5, 0, &hci_cmd[1]);
                // mp_obj_t status = mp_call_method_n_kw(4, 0, hci_cmd);
                if (mp_obj_get_int(status) == 0) {
                    mp_buffer_info_t bufinfo = {0};
                    mp_get_buffer_raise(rsp_ba, &bufinfo, MP_BUFFER_READ);
                    DEBUG_printf("rsp: len 0x%x\n", bufinfo.len);
                    DEBUG_printf("rsp: 0 0x%x\n", ((char*)bufinfo.buf)[0]);
                    DEBUG_printf("rsp: 1 0x%x\n", ((char*)bufinfo.buf)[1]);
                    DEBUG_printf("rsp: 2 0x%x\n", ((char*)bufinfo.buf)[2]);
                    mpstream_write((const char *)bufinfo.buf, bufinfo.len);

                    mpy_cb(false);
                } else {
                    DEBUG_printf("rsp: 0x%x", mp_obj_get_int(status));
                }
            }

            rx = 0;
            len = 0;
            state = STATE_IDLE;

        } else if (mpstream_poll(MP_STREAM_POLL_RD) & MP_STREAM_POLL_RD) {
            uint8_t c;
            int32_t ret = mpstream_read(&c, 1);
            if (ret < 1) {
                continue;
            }
            mpy_cb(true);

            if (state == STATE_IDLE) {
                if (c == HCI_KIND_BT_CMD || c == HCI_KIND_BT_ACL || c == HCI_KIND_BT_EVENT || c == HCI_KIND_VENDOR_RESPONSE || c == HCI_KIND_VENDOR_EVENT || HCI_KIND_LOCAL_CMD) {
                    cmd_type = c;
                    state = STATE_NEED_LEN;
                    buf[rx++] = c;
                    len = 0;
                    DEBUG_printf("cmd_type 0x%x\n", c);
                } else {
                    DEBUG_printf("cmd_type unknown 0x%x\n", c);
                    // __asm__ ("BKPT");
                }
            } else if (state == STATE_NEED_LEN) {
                buf[rx++] = c;
                if (cmd_type == HCI_KIND_BT_ACL && rx == 4) {
                    len = c;
                }
                if (cmd_type == HCI_KIND_BT_ACL && rx == 5) {
                    len += ((size_t)c) << 8;
                    DEBUG_printf("len 0x%x\n", c);
                    state = STATE_IN_PAYLOAD;
                }
                if (cmd_type == HCI_KIND_BT_EVENT && rx == 3) {
                    len = c;
                    DEBUG_printf("len 0x%x\n", c);
                    state = STATE_IN_PAYLOAD;
                }
                if (cmd_type == HCI_KIND_BT_CMD && rx == 4) {
                    len = c;
                    DEBUG_printf("len 0x%x\n", c);
                    state = STATE_IN_PAYLOAD;
                }
                if (cmd_type == HCI_KIND_LOCAL_CMD && rx == 4) {
                    len = c;
                    DEBUG_printf("len 0x%x\n", c);
                    state = STATE_IN_PAYLOAD;
                }
            } else if (state == STATE_IN_PAYLOAD) {
                buf[rx++] = c;
                --len;
            }
        }

    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rfcore_transparent_obj, 4, 4, rfcore_transparent);


mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    // This must be first, it sets up the globals dict and other things
    MP_DYNRUNTIME_INIT_ENTRY

    // Make the function available in the module's namespace
    mp_store_global(MP_QSTR__start, MP_OBJ_FROM_PTR(&rfcore_transparent_obj));

    // This must be last, it restores the globals dict
    MP_DYNRUNTIME_INIT_EXIT
}
