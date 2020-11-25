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
    print(*args)


class BlePairing:
    KEYSTORE_OUR_SEC = []
    KEYSTORE_PEER_SEC = []
    KEYSTORE_CCCD = []

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
            json.dump({
                'KEYSTORE_OUR_SEC': self._bytes_to_str(BlePairing.KEYSTORE_OUR_SEC),
                'KEYSTORE_PEER_SEC': self._bytes_to_str(BlePairing.KEYSTORE_PEER_SEC),
                'KEYSTORE_CCCD': self._bytes_to_str(BlePairing.KEYSTORE_CCCD),
            }, f)
        self.print_store()

    def read_store(self):
        try:
            with open(self.store, "rb") as f:
                keystore = json.load(f)
                BlePairing.KEYSTORE_OUR_SEC = self._str_to_bytes(keystore['KEYSTORE_OUR_SEC'])
                BlePairing.KEYSTORE_PEER_SEC = self._str_to_bytes(keystore['KEYSTORE_PEER_SEC'])
                BlePairing.KEYSTORE_CCCD = self._str_to_bytes(keystore['KEYSTORE_CCCD'])
            self.print_store()
        except (ValueError, AttributeError, KeyError) as ex:
            print("BLE Pairing: error in %s (%s), not loaded" % (self.store, ex))
        except OSError:
            # Likely file doesn't exist yet
            pass

    def print_store(self):
        print_debug("********")
        print_debug('KEYSTORE_OUR_SEC:', ",\n  ".join(str(BlePairing.KEYSTORE_OUR_SEC).split(',')))
        print_debug('KEYSTORE_PEER_SEC:', ",\n  ".join(str(BlePairing.KEYSTORE_PEER_SEC).split(',')))
        print_debug('KEYSTORE_CCCD:', ",\n  ".join(str(BlePairing.KEYSTORE_CCCD).split(',')))
        print_debug("********")

    @staticmethod
    def find_key(key, delete=False):
        bond_type = key['bond_type']
        ignore_addr = key['addr_type'] == 0 and key['addr'] == b'\x00\x00\x00\x00\x00\x00'
        ediv_rand_present = None in (key.get('ediv'), key.get('rand'))

        if bond_type == BOND_TYPE_OUR_SEC:
            ks = BlePairing.KEYSTORE_OUR_SEC
            match = [] if ignore_addr else ['addr_type', 'addr']
            if ediv_rand_present:
                match.extend(['ediv', 'rand'])
            fields = SEC_FIELDS

        elif bond_type == BOND_TYPE_PEER_SEC:
            ks = BlePairing.KEYSTORE_PEER_SEC
            match = [] if ignore_addr else ['addr_type', 'addr']
            if ediv_rand_present:
                match.extend(['ediv', 'rand'])
            fields = SEC_FIELDS

        elif bond_type == BOND_TYPE_CCCD:
            ks = BlePairing.KEYSTORE_CCCD
            match = ['chr_val_handle'] if ignore_addr else ['addr_type', 'addr', 'chr_val_handle']
            fields = CCCD_FIELDS

        else:
            raise ValueError("Unknown bond type: %s" % bond_type)

        ret = None
        search = tuple((key[k] for k in match))
        
        skipped = 0
        for entry in ks:
            if search != tuple((entry[k] for k in match)):
                continue

            if key.get('skip', 0) > skipped:
                skipped += 1
                continue
            ret = entry
            break

        if ret is not None:
            if delete:
                ks.remove(ret)
            print_debug("FOUND", ret)
            return tuple((ret[k] for k in fields[1:]))
        print_debug("NOT FOUND IN", ks)
        

    def irq(self, event, data):
        if event == _IRQ_BOND_READ:
            print_debug("_IRQ_BOND_READ")
            key = dict(zip(KEY_FIELDS, [bytes(d) if isinstance(d, memoryview) else d for d in data]))
            print_debug(key)
            found = self.find_key(key)
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
            self.find_key(details, delete=True)  # remove any existing key for same peer
            ks.append(details)
            print("BLE: Writing bond key")
            schedule(self.write_store, None)

        elif event == _IRQ_BOND_DELETE:
            print_debug("_IRQ_BOND_DELETE")
            key = dict(zip(KEY_FIELDS, [bytes(d) if isinstance(d, memoryview) else d for d in data]))
            print_debug(key)
            found = self.find_key(key, delete=True)
            if found:
                print("BLE: Deleting old bond")
                schedule(self.write_store, None)
            return found
