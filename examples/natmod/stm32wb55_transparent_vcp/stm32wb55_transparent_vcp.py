# import stm


def start():
    import sys
    import micropython
    from bluetooth import BLE
    from pyb import LED

    # Ensure rfcore has been started at least once, then turn off bluetooth
    BLE().active(1)
    BLE().active(0)
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
    _start(usb, usb, callback)


# BLE().active(0)
# micropython.kbd_intr(-1)
# stm.rfcore_transparent(sys.stdin, sys.stdout)
