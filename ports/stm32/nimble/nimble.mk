# Makefile directives for Apache mynewt nimble BLE component

SRC_MOD += \
	modnimble.c \

NIMBLE_DIR = lib/mynewt-nimble
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/nimble/host/src/, \
	ble_att.c \
	ble_att_clt.c \
	ble_att_cmd.c \
	ble_att_svr.c \
	ble_eddystone.c \
	ble_gap.c \
	ble_gattc.c \
	ble_gatts.c \
	ble_hs_adv.c \
	ble_hs_atomic.c \
	ble_hs.c \
	ble_hs_cfg.c \
	ble_hs_conn.c \
	ble_hs_dbg.c \
	ble_hs_flow.c \
	ble_hs_hci.c \
	ble_hs_hci_cmd.c \
	ble_hs_hci_evt.c \
	ble_hs_hci_util.c \
	ble_hs_id.c \
	ble_hs_log.c \
	ble_hs_mbuf.c \
	ble_hs_misc.c \
	ble_hs_mqueue.c \
	ble_hs_pvcy.c \
	ble_hs_startup.c \
	ble_hs_stop.c \
	ble_ibeacon.c \
	ble_l2cap.c \
	ble_l2cap_coc.c \
	ble_l2cap_sig.c \
	ble_l2cap_sig_cmd.c \
	ble_monitor.c \
	ble_sm_alg.c \
	ble_sm.c \
	ble_sm_cmd.c \
	ble_sm_lgcy.c \
	ble_sm_sc.c \
	ble_store.c \
	ble_store_util.c \
	ble_uuid.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/nimble/host/services/gap/src/, \
	ble_svc_gap.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/nimble/host/services/gatt/src/, \
	ble_svc_gatt.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/nimble/host/store/ram/src/, \
	ble_store_ram.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/nimble/host/util/src/, \
	addr.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/nimble/transport/uart/src/, \
	ble_hci_uart.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/porting/nimble/src/, \
	endian.c \
	mem.c \
	nimble_port.c \
	os_mbuf.c \
	os_mempool.c \
	os_msys_init.c \
	)
SRC_NIMBLE += $(addprefix $(NIMBLE_DIR)/ext/tinycrypt/src/, \
	aes_encrypt.c \
	cmac_mode.c \
	ecc.c \
	ecc_dh.c \
	utils.c \
	)
SRC_NIMBLE += \
	nimble/npl_os.c \
	nimble/hci_uart.c \
	nimble/nimble_nus.c \

INC += -Inimble # for bsp/bsp.h, hal/hal_gpio.h, hal/hal_uart.h
INC += -I$(TOP)/$(NIMBLE_DIR) # for nimble/host/src/ble_hs_hci_priv.h"
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/include
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/host/include
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/host/services/gap/include
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/host/services/gatt/include
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/host/store/ram/include
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/host/util/include
INC += -I$(TOP)/$(NIMBLE_DIR)/nimble/transport/uart/include
INC += -I$(TOP)/$(NIMBLE_DIR)/porting/nimble/include
INC += -I$(TOP)/$(NIMBLE_DIR)/ext/tinycrypt/include

CFLAGS += -Wno-unused-but-set-variable -Wno-pointer-arith -Wno-error=maybe-uninitialized -Wno-unused-variable -Wno-error=format

OBJ += $(addprefix $(BUILD)/, $(SRC_NIMBLE:.c=.o))
