# Makefile directives for Apache Mynewt Zephyr component

ifeq ($(MICROPY_BLUETOOTH_ZEPHYR),1)

EXTMOD_DIR = extmod
ZEPHYR_EXTMOD_DIR = $(EXTMOD_DIR)/zephyr_ble

EXTMOD_SRC_C += $(ZEPHYR_EXTMOD_DIR)/modbluetooth_zephyr.c

CFLAGS_MOD += -DMICROPY_BLUETOOTH_ZEPHYR=1

# On all ports where we provide the full implementation (i.e. not just
# bindings like on ESP32), then we don't need to use the ringbuffer. In this
# case, all Zephyr events are run by the MicroPython scheduler. On Unix, the
# scheduler is also responsible for polling the UART, whereas on STM32 the
# UART is also polled by the RX IRQ.
CFLAGS_MOD += -DMICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS=1

# Without the ringbuffer, and with the full implementation, we can also
# enable pairing and bonding. This requires both synchronous events and
# some customisation of the key store.
CFLAGS_MOD += -DMICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING=1

ZEPHYR_LIB_DIR = lib/zephyr

# LIB_SRC_C += $(addprefix $(ZEPHYR_LIB_DIR)/, \
# 	$(addprefix ext/tinycrypt/src/, \
# 		aes_encrypt.c \
# 		cmac_mode.c \
# 		ecc.c \
# 		ecc_dh.c \
# 		utils.c \
# 		) \
# 	zephyr/host/services/gap/src/ble_svc_gap.c \
# 	zephyr/host/services/gatt/src/ble_svc_gatt.c \
# 	$(addprefix zephyr/host/src/, \
# 		ble_att.c \
# 		ble_att_clt.c \
# 		ble_att_cmd.c \
# 		ble_att_svr.c \
# 		ble_eddystone.c \
# 		ble_gap.c \
# 		ble_gattc.c \
# 		ble_gatts.c \
# 		ble_hs_adv.c \
# 		ble_hs_atomic.c \
# 		ble_hs.c \
# 		ble_hs_cfg.c \
# 		ble_hs_conn.c \
# 		ble_hs_flow.c \
# 		ble_hs_hci.c \
# 		ble_hs_hci_cmd.c \
# 		ble_hs_hci_evt.c \
# 		ble_hs_hci_util.c \
# 		ble_hs_id.c \
# 		ble_hs_log.c \
# 		ble_hs_mbuf.c \
# 		ble_hs_misc.c \
# 		ble_hs_mqueue.c \
# 		ble_hs_pvcy.c \
# 		ble_hs_startup.c \
# 		ble_hs_stop.c \
# 		ble_ibeacon.c \
# 		ble_l2cap.c \
# 		ble_l2cap_coc.c \
# 		ble_l2cap_sig.c \
# 		ble_l2cap_sig_cmd.c \
# 		ble_monitor.c \
# 		ble_sm_alg.c \
# 		ble_sm.c \
# 		ble_sm_cmd.c \
# 		ble_sm_lgcy.c \
# 		ble_sm_sc.c \
# 		ble_store.c \
# 		ble_store_util.c \
# 		ble_uuid.c \
# 		) \
# 	zephyr/host/util/src/addr.c \
# 	zephyr/transport/uart/src/ble_hci_uart.c \
# 	$(addprefix porting/zephyr/src/, \
# 		endian.c \
# 		mem.c \
# 		zephyr_port.c \
# 		os_mbuf.c \
# 		os_mempool.c \
# 		os_msys_init.c \
# 		) \
# 	)
# 	# zephyr/host/store/ram/src/ble_store_ram.c \

# EXTMOD_SRC_C += $(addprefix $(ZEPHYR_EXTMOD_DIR)/, \
# 	zephyr/zephyr_npl_os.c \
# 	hal/hal_uart.c \
# 	)

INC += -I$(TOP)/$(ZEPHYR_EXTMOD_DIR)
INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/include/zephyr
INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/ext/tinycrypt/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/host/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/host/services/gap/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/host/services/gatt/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/host/store/ram/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/host/util/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/zephyr/transport/uart/include
# INC += -I$(TOP)/$(ZEPHYR_LIB_DIR)/porting/zephyr/include

# $(BUILD)/$(ZEPHYR_LIB_DIR)/%.o: CFLAGS += -Wno-maybe-uninitialized -Wno-pointer-arith -Wno-unused-but-set-variable -Wno-format -Wno-sign-compare -Wno-old-style-declaration

endif
