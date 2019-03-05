#include <string.h>

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "extmod/vfs.h"
#include "pin_static_af.h"
#include "uart.h"
#include "nimble/ble.h"
#include "hal/hal_uart.h"

#if MICROPY_PY_NIMBLE

#define NIMBLE_UART_ID 6
#define NIMBLE_PIN_PWR pyb_pin_BT_REG_ON
#define NIMBLE_PIN_CTS pyb_pin_BT_CTS

/******************************************************************************/
// UART

#define hci_uart_obj (&hci_uart_obj_s)

static pyb_uart_obj_t hci_uart_obj_s;
static uint8_t hci_uart_rxbuf[512];

static int uart_init_(void) {
    hci_uart_obj_s.base.type = &pyb_uart_type;
    hci_uart_obj_s.uart_id = NIMBLE_UART_ID;
    hci_uart_obj_s.is_static = true;
    hci_uart_obj_s.timeout = 2;
    hci_uart_obj_s.timeout_char = 2;
    MP_STATE_PORT(pyb_uart_obj_all)[hci_uart_obj_s.uart_id - 1] = &hci_uart_obj_s;
    uart_init(&hci_uart_obj_s, 115200, UART_WORDLENGTH_8B, UART_PARITY_NONE, UART_STOPBITS_1, UART_HWCONTROL_RTS | UART_HWCONTROL_CTS);
    uart_set_rxbuf(&hci_uart_obj_s, sizeof(hci_uart_rxbuf), hci_uart_rxbuf);
    return 0;
}

static int uart_set_baudrate(uint32_t baudrate) {
    uart_init(&hci_uart_obj_s, baudrate, UART_WORDLENGTH_8B, UART_PARITY_NONE, UART_STOPBITS_1, UART_HWCONTROL_RTS | UART_HWCONTROL_CTS);
    uart_set_rxbuf(&hci_uart_obj_s, sizeof(hci_uart_rxbuf), hci_uart_rxbuf);
    return 0;
}

/******************************************************************************/
// BT HCI low-level driver

static uint8_t hci_cmd_buf[4 + 256];

static void bthci_wait_cts_low(void) {
    mp_hal_pin_config(NIMBLE_PIN_CTS, MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_UP, 0);
    for (int i = 0; i < 200; ++i) {
        if (mp_hal_pin_read(NIMBLE_PIN_CTS) == 0) {
            break;
        }
        mp_hal_delay_ms(1);
    }
    mp_hal_pin_config_alt_static(NIMBLE_PIN_CTS, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_UP, STATIC_AF_USART6_CTS);
}

static int bthci_hci_cmd_raw(size_t len, uint8_t *buf) {
    uart_tx_strn(hci_uart_obj, (void*)buf, len);
    for (int i = 0; i < 6; ++i) {
        while (!uart_rx_any(hci_uart_obj)) {
            MICROPY_EVENT_POLL_HOOK
        }
        buf[i] = uart_rx_char(hci_uart_obj);
    }

    // expect a comand complete event (event 0x0e)
    if (buf[0] != 0x04 || buf[1] != 0x0e) {
        printf("unknown response: %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
        return -1;
    }

    int sz = buf[2] - 3;
    for (int i = 0; i < sz; ++i) {
        while (!uart_rx_any(hci_uart_obj)) {
            MICROPY_EVENT_POLL_HOOK
        }
        buf[i] = uart_rx_char(hci_uart_obj);
    }

    return 0;
}

static int bthci_hci_cmd(int ogf, int ocf, size_t param_len, const uint8_t *param_buf) {
    uint8_t *buf = hci_cmd_buf;
    buf[0] = 0x01;
    buf[1] = ocf;
    buf[2] = ogf << 2 | ocf >> 8;
    buf[3] = param_len;
    if (param_len) {
        memcpy(buf + 4, param_buf, param_len);
    }
    return bthci_hci_cmd_raw(4 + param_len, buf);
}

static int bthci_set_baudrate(uint32_t baudrate) {
    uint8_t buf[6];
    put_le16(buf, 0);
    put_le32(buf + 2, baudrate);
    // note: this is a vendor-specific HCI command
    return bthci_hci_cmd(0x3f, 0x18, 6, buf);
}

static int bthci_init(void) {
    mp_hal_pin_output(NIMBLE_PIN_PWR);
    mp_hal_pin_low(NIMBLE_PIN_PWR);

    return 0;
}

static int bthci_activate(void) {
    uint8_t buf[256];

    mp_hal_pin_low(NIMBLE_PIN_PWR);
    uart_set_baudrate(115200);
    mp_hal_delay_ms(100);
    mp_hal_pin_high(NIMBLE_PIN_PWR);
    bthci_wait_cts_low();

    // reset
    bthci_hci_cmd(0x03, 0x0003, 0, NULL);

    // change baudrate
    bthci_set_baudrate(3000000);
    uart_set_baudrate(3000000);

    // reset
    bthci_hci_cmd(0x03, 0x0003, 0, NULL);

    // set BD_ADDR (sent as little endian)
    // note: this is a vendor-specific HCI command
    uint8_t bdaddr[6];
    mp_hal_get_mac(MP_HAL_MAC_BDADDR, bdaddr);
    buf[0] = bdaddr[5];
    buf[1] = bdaddr[4];
    buf[2] = bdaddr[3];
    buf[3] = bdaddr[2];
    buf[4] = bdaddr[1];
    buf[5] = bdaddr[0];
    bthci_hci_cmd(0x3f, 0x0001, 6, buf);

    // set local name
    memset(buf, 0, 248);
    memcpy(buf, "PYB-BLE", 8);
    bthci_hci_cmd(0x03, 0x0013, 248, buf);

    // HCI_Write_LE_Host_Support
    bthci_hci_cmd(3, 109, 2, (const uint8_t*)"\x01\x00");

    return 0;
}

/******************************************************************************/
// Bindings to Nimble

static hal_uart_tx_cb_t hal_uart_tx_cb;
static void *hal_uart_tx_arg;
static hal_uart_rx_cb_t hal_uart_rx_cb;
static void *hal_uart_rx_arg;

static uint32_t bt_sleep_ticks;

int hal_uart_init_cbs(uint32_t port, hal_uart_tx_cb_t tx_cb, void *tx_arg, hal_uart_rx_cb_t rx_cb, void *rx_arg) {
    hal_uart_tx_cb = tx_cb;
    hal_uart_tx_arg = tx_arg;
    hal_uart_rx_cb = rx_cb;
    hal_uart_rx_arg = rx_arg;
    return 0; // success
}

int hal_uart_config(uint32_t port, uint32_t baud, uint32_t bits, uint32_t stop, uint32_t parity, uint32_t flow) {
    uart_init_();
    bthci_init();
    bthci_activate();
    return 0; // success
}

void hal_uart_start_tx(uint32_t port) {
    size_t len = 0;
    for (;;) {
        int data = hal_uart_tx_cb(hal_uart_tx_arg);
        if (data == -1) {
            break;
        }
        hci_cmd_buf[len++] = data;
    }
    uart_tx_strn(hci_uart_obj, (void*)hci_cmd_buf, len);
}

int hal_uart_close(uint32_t port) {
    return 0; // success
}

void nimble_uart_process(void) {
    while (uart_rx_any(hci_uart_obj)) {
        uint8_t data = uart_rx_char(hci_uart_obj);
        hal_uart_rx_cb(hal_uart_rx_arg, data);
    }
}

#endif // MICROPY_PY_NIMBLE
