# This example demonstrates a simple temperature sensor peripheral.
#
# The sensor's local value updates every second, and it will notify
# any connected central every 10 seconds.

import bluetooth
import random
import struct
import time
from ble_advertising import advertising_payload

from micropython import const

_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)
_IRQ_GATTS_INDICATE_DONE = const(20)

_IRQ_GET_SECRET = const(29)
_IRQ_SET_SECRET = const(30)

_FLAG_READ = const(0x0002)
_FLAG_NOTIFY = const(0x0010)
_FLAG_INDICATE = const(0x0020)

_FLAG_READ_ENCRYPTED = const(0x0200)

# org.bluetooth.service.environmental_sensing
_ENV_SENSE_UUID = bluetooth.UUID(0x181A)
# org.bluetooth.characteristic.temperature
_TEMP_CHAR = (
    bluetooth.UUID(0x2A6E),
    _FLAG_READ | _FLAG_NOTIFY | _FLAG_INDICATE | _FLAG_READ_ENCRYPTED,
)
_ENV_SENSE_SERVICE = (
    _ENV_SENSE_UUID,
    (_TEMP_CHAR,),
)

# org.bluetooth.characteristic.gap.appearance.xml
_ADV_APPEARANCE_GENERIC_THERMOMETER = const(768)


class BLETemperature:
    def __init__(self, ble, name="mpy-temp"):
        self._ble = ble
        self._secrets = {
            (2, b'\x00\xff\xcd\x9e6N@'): b'\x00\xff\xcd\x9e6N@\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc2f\xcfB\x0c\xd7\xfb\x17S\xbf"\x1eO\xeb\xcdf\x01\x16\xfc\xe9\xd190\x84y\xb4\x1d\x05\xa6\xb1LJo\x01E\x02\x17RT\x1a6x\xd5\x10Q\x9f@\xd4\x94\xa7\x05\x00\x00\x00\x00\x00',
            (1, b'\x00\xff\xcd\x9e6N@'): b'\x00\xff\xcd\x9e6N@\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc2f\xcfB\x0c\xd7\xfb\x17S\xbf"\x1eO\xeb\xcdf\x01\xef\x8d\xe2\x16O\xecC\r\xbf[\xdd4\xc0S\x1e\xb8\x01kJ/\xea\xbf\x1e\x84\xc3B\xb5M*\xb1&4h\x05\x00\x00\x00\x00\x00'
        }

        self._ble.irq(self._irq)
        self._ble.config(bond=True)
        self._ble.active(True)
        self._ble.config(addr_mode=2)
        ((self._handle,),) = self._ble.gatts_register_services((_ENV_SENSE_SERVICE,))
        self._connections = set()
        self._payload = advertising_payload(
            name=name, services=[_ENV_SENSE_UUID], appearance=_ADV_APPEARANCE_GENERIC_THERMOMETER
        )
        self._advertise()

    def _irq(self, event, data):
        # Track connections so we can send notifications.
        if event == _IRQ_CENTRAL_CONNECT:
            conn_handle, _, _ = data
            self._connections.add(conn_handle)
        elif event == _IRQ_CENTRAL_DISCONNECT:
            conn_handle, _, _ = data
            self._connections.remove(conn_handle)
            # Start advertising again to allow a new connection.
            self._advertise()
        elif event == _IRQ_GATTS_INDICATE_DONE:
            conn_handle, value_handle, status = data
        elif event == _IRQ_SET_SECRET:
            sec_type, key, value = data
            print('set secret:', sec_type, bytes(key), bytes(value) if value else None)
            if value is None:
                if (sec_type, bytes(key)) in self._secrets:
                    del self._secrets[sec_type, bytes(key)]
                    return True
                else:
                    return False
            else:
                self._secrets[sec_type, bytes(key)] = bytes(value)
            return True
        elif event == _IRQ_GET_SECRET:
            sec_type, index, key = data
            print('get secret:', sec_type, index, bytes(key) if key else None)
            i = 0
            if key is None:
                for (t, _key), value in self._secrets.items():
                    if t == sec_type:
                        if i == index:
                            return value
                return None
            else:
                return self._secrets.get((sec_type, bytes(key)), None)

    def set_temperature(self, temp_deg_c, notify=False, indicate=False):
        # Data is sint16 in degrees Celsius with a resolution of 0.01 degrees Celsius.
        # Write the local value, ready for a central to read.
        self._ble.gatts_write(self._handle, struct.pack("<h", int(temp_deg_c * 100)))
        if notify or indicate:
            for conn_handle in self._connections:
                if notify:
                    # Notify connected centrals.
                    self._ble.gatts_notify(conn_handle, self._handle)
                if indicate:
                    # Indicate connected centrals.
                    self._ble.gatts_indicate(conn_handle, self._handle)

    def _advertise(self, interval_us=500000):
        self._ble.config(addr_mode=2)
        self._ble.gap_advertise(interval_us, adv_data=self._payload)


def demo():
    ble = bluetooth.BLE()
    temp = BLETemperature(ble)

    t = 25
    i = 0

    while True:
        # Write every second, notify every 10 seconds.
        i = (i + 1) % 10
        temp.set_temperature(t, notify=i == 0, indicate=False)
        # Random walk the temperature.
        t += random.uniform(-0.5, 0.5)
        time.sleep_ms(1000)


if __name__ == "__main__":
    demo()
