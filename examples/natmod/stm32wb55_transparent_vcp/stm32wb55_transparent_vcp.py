# import stm


def start():
    import sys
    import micropython
    from bluetooth import BLE
    from pyb import LED

    ble = BLE()

    try:
        ble.hci_cmd
    except AttributeError:
        raise AttributeError("micropython board/build with MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD enabled required.")

    # Ensure rfcore has been started at least once, then turn off bluetooth
    ble.active(1)
    ble.active(0)
    # Disable the ctrl-c interrupt
    import os

    usb = os.dupterm(None, 1)  # startup default is usb (repl) on slot 1

    micropython.kbd_intr(-1)
    # start transparant mode C function (never exits)
    def callback(status):
        if status:
            LED(3).on()
        else:
            LED(3).off()

    # _start(sys.stdin, sys.stdout, callback)
    _start(ble, usb, usb, callback)


# BLE().active(0)
# micropython.kbd_intr(-1)
# stm.rfcore_transparent(sys.stdin, sys.stdout)
