# Helpers for handling BLE pairing and bonding.

import json
import ubinascii
from micropython import const, schedule

_IRQ_BOND_READ = const(29)
_IRQ_BOND_WRITE = const(30)
_IRQ_BOND_DELETE = const(31)

BOND_TYPE_OUR_SEC = const(1)
BOND_TYPE_PEER_SEC = const(2)
BOND_TYPE_CCCD = const(3)

KEY_FIELDS = ('bond_type', 'addr_type', 'addr', 'ediv', 'rand', 'chr_val_handle', 'skip')
SEC_FIELDS = ('bond_type', 'addr_type', 'addr', 'key_size', 'ediv', 'rand', 'ltk', 'irk', 'csrk', 'authenticated',
              'secure_connection')
CCCD_FIELDS = ('bond_type', 'addr_type', 'addr', 'chr_val_handle', 'flags', 'value_changed')


def print_debug(*args):
    """ verbose logging """
    # print(*args)


class BlePairing:
    KEYSTORE_OUR_SEC = []
    KEYSTORE_PEER_SEC = []
    KEYSTORE_CCCD = []

    def __init__(self, filename):
        self.store = filename
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
            json.dump({
                'KEYSTORE_OUR_SEC': self._bytes_to_str(BlePairing.KEYSTORE_OUR_SEC),
                'KEYSTORE_PEER_SEC': self._bytes_to_str(BlePairing.KEYSTORE_PEER_SEC),
                'KEYSTORE_CCCD': self._bytes_to_str(BlePairing.KEYSTORE_CCCD),
            }, f)

    def read_store(self):
        try:
            with open(self.store, "rb") as f:
                keystore = json.load(f)
                BlePairing.KEYSTORE_OUR_SEC = self._str_to_bytes(keystore['KEYSTORE_OUR_SEC'])
                BlePairing.KEYSTORE_PEER_SEC = self._str_to_bytes(keystore['KEYSTORE_PEER_SEC'])
                BlePairing.KEYSTORE_CCCD = self._str_to_bytes(keystore['KEYSTORE_CCCD'])
        except (ValueError, AttributeError):
            print("format error in %s, not loaded" % self.store)
        except OSError:
            pass

    @staticmethod
    def find_key(key, delete=False):
        bond_type = key['bond_type']
        if bond_type == BOND_TYPE_OUR_SEC:
            ks = BlePairing.KEYSTORE_OUR_SEC
            match = ('addr_type', 'addr', 'ediv', 'rand')
            fields = SEC_FIELDS

        elif bond_type == BOND_TYPE_PEER_SEC:
            ks = BlePairing.KEYSTORE_PEER_SEC
            match = ('addr_type', 'addr', 'ediv', 'rand')
            fields = SEC_FIELDS

        elif bond_type == BOND_TYPE_CCCD:
            ks = BlePairing.KEYSTORE_CCCD
            match = ('addr_type', 'addr', 'chr_val_handle')
            fields = CCCD_FIELDS

        else:
            raise ValueError("Unknown bond type: %s" % bond_type)

        ret = None
        search = tuple((key[k] for k in match))
        for entry in ks:
            if key.get('skip'):
                key['skip'] -= 1
                continue

            if search == tuple((entry[k] for k in match)):
                ret = entry
                break

        if ret is not None:
            if delete:
                ks.remove(ret)
            return tuple((ret[k] for k in fields[1:]))

    def irq(self, event, data):
        if event == _IRQ_BOND_READ:
            print_debug("_IRQ_BOND_READ")
            key = dict(zip(KEY_FIELDS, [bytes(d) if isinstance(d, memoryview) else d for d in data]))
            print_debug(key)
            found = self.find_key(key)
            print_debug("FOUND", found)
            return found

        elif event == _IRQ_BOND_WRITE:
            print_debug("_IRQ_BOND_WRITE", data)
            bond_type = data[0]
            if bond_type == BOND_TYPE_OUR_SEC:
                ks = BlePairing.KEYSTORE_OUR_SEC
                fields = SEC_FIELDS
            elif bond_type == BOND_TYPE_PEER_SEC:
                ks = BlePairing.KEYSTORE_PEER_SEC
                fields = SEC_FIELDS
            elif bond_type == BOND_TYPE_CCCD:
                ks = BlePairing.KEYSTORE_CCCD
                fields = CCCD_FIELDS
            else:
                raise ValueError("Unknown bond type: %s" % bond_type)
            details = dict(zip(fields, [bytes(d) if isinstance(d, memoryview) else d for d in data]))
            print_debug(details)
            ks.append(details)
            print("BLE: Writing bond key")
            schedule(self.write_store, None)

        elif event == _IRQ_BOND_DELETE:
            print_debug("_IRQ_BOND_DELETE")
            key = dict(zip(KEY_FIELDS, [bytes(d) if isinstance(d, memoryview) else d for d in data]))
            print_debug(key)
            found = self.find_key(key, delete=True)
            print_debug("FOUND", found)
            if found:
                print("BLE: Deleting old bond")
                schedule(self.write_store, None)
            return found
