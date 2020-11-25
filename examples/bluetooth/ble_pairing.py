# Helpers for handling BLE pairing and bonding.

import json
import ubinascii
from micropython import const, schedule

_IRQ_GET_SECRET = const(29)
_IRQ_SET_SECRET = const(30)

def print_debug(*args):
    """ verbose logging """
    print(*args)


class BlePairing:
    KEYSTORE = {}

    def __init__(self, filename):
        self.store = str(filename)
        self.read_store()

    @staticmethod
    def _bytes_to_str(ks):
        return [
            {k: ubinascii.b2a_base64(v) if isinstance(v, bytes) else v for k, v in entry.items()}
            for entry in ks
        ]

    @staticmethod
    def _str_to_bytes(ks):
        return [
            {k: ubinascii.a2b_base64(v) if isinstance(v, str) else v for k, v in entry.items()}
            for entry in ks
        ]

    def write_store(self, *_):
        with open(self.store, "wb") as f:
            detail = []
            for (ki, kb), blob in BlePairing.KEYSTORE.items():
                detail.append([ki, ubinascii.b2a_base64(kb), ubinascii.b2a_base64(blob)])

            json.dump(detail, f)
        self.print_store()

    def read_store(self):
        try:
            with open(self.store, "rb") as f:
                loaded = json.load(f)
                keystore = {}
                for ki, kb, blob in loaded:
                    keystore[(ki, ubinascii.a2b_base64(kb))] = ubinascii.a2b_base64(blob)

                BlePairing.KEYSTORE = keystore
            self.print_store()
        except (ValueError, AttributeError, KeyError) as ex:
            print("BLE Pairing: error in %s (%s), not loaded" % (self.store, ex))
        except OSError:
            # Likely file doesn't exist yet
            pass

    def print_store(self):
        print_debug("********")
        print_debug('KEYSTORE:', ", (\n  ".join(str(BlePairing.KEYSTORE).split('),')))
        print_debug("********")

    def irq(self, event, data):
        if event == _IRQ_SET_SECRET:
            sec_type, key, value = data
            print('set secret:', sec_type, bytes(key), bytes(value) if value else None)
            if value is None:
                if (sec_type, bytes(key)) in BlePairing.KEYSTORE:
                    del BlePairing.KEYSTORE[sec_type, bytes(key)]
                    schedule(self.write_store, None)
                    return True
                else:
                    return False
            else:
                BlePairing.KEYSTORE[sec_type, bytes(key)] = bytes(value)
                schedule(self.write_store, None)
            return True

        elif event == _IRQ_GET_SECRET:
            sec_type, index, key = data
            print('get secret:', sec_type, index, bytes(key) if key else None)
            i = 0
            if key is None:
                for (t, _key), value in BlePairing.KEYSTORE.items():
                    if t == sec_type:
                        if i == index:
                            return value
                return None
            else:
                return BlePairing.KEYSTORE.get((sec_type, bytes(key)), None)
