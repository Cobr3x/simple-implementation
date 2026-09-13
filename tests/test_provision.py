# Generative AI disclosure: Drafted with Microsoft Copilot; revised with OpenAI Codex (SOL model).

import hashlib
import hmac
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('provision', Path(__file__).resolve().parents[1] / 'tools/provision.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


def decode_hex(path):
    memory, base = {}, 0
    for line in path.read_text().splitlines():
        raw = bytes.fromhex(line[1:])
        assert sum(raw) % 256 == 0
        size, address, kind = raw[0], int.from_bytes(raw[1:3], 'big'), raw[3]
        assert len(raw) == size + 5
        if kind == 4:
            base = int.from_bytes(raw[4:-1], 'big') << 16
        elif kind == 0:
            for i, value in enumerate(raw[4:-1]):
                assert base + address + i not in memory
                memory[base + address + i] = value
        else:
            assert kind == 1
    return memory


class ProvisionTests(unittest.TestCase):
    def test_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            image = struct.pack('<II', 0x20010000, 0x08000009) + b'approved firmware'
            g431_image = struct.pack('<II', 0x20008000, 0x08000009) + b'G431 verifier'
            (root / 'image.bin').write_bytes(image)
            (root / 'g431.bin').write_bytes(g431_image)
            (root / 'keys.json').write_text(json.dumps({'k_auth': '01' * 32, 'k_attest': '02' * 32}))
            manifest = p.generate(root / 'image.bin', root / 'g431.bin', root / 'pair', root / 'keys.json')
            memory = decode_hex(root / 'pair/h743.hex')
            measured = (root / 'pair/h743-measured.bin').read_bytes()
            self.assertEqual(measured, image.ljust(p.MEASURE_LENGTH, b'\xff'))
            self.assertNotIn(p.MEASURE_START + len(image), memory)
            self.assertEqual(manifest['reference_vs'], hmac.digest(b'\x02' * 32, measured, 'sha256').hex())
            self.assertEqual((root / 'pair/g431-firmware-region.bin').read_bytes(),
                             g431_image.ljust(p.BOARDS['g431']['image_length'], b'\xff'))
            self.assertEqual((root / 'pair/h743-provision.bin').stat().st_size, 160)
            self.assertEqual((root / 'pair/g431-provision.bin').stat().st_size, 160)
            self.assertEqual((root / 'pair/h743-journal-a.bin').stat().st_size, 32)
            self.assertEqual((root / 'pair/g431-journal-b.bin').stat().st_size, 32)
            for board, config in p.BOARDS.items():
                mem = decode_hex(root / f'pair/{board}.hex')
                material = bytes(mem[config['keys'] + i] for i in range(160))
                self.assertEqual(material[12:16], struct.pack('>I', config['role']))
                self.assertEqual(material[124:156], hashlib.sha256(material[:124]).digest())
                for address in config['journals']:
                    record = bytes(mem[address + i] for i in range(32))
                    self.assertEqual(record[:8], struct.pack('>II', 1, 0xFFFFFFFE))
                    self.assertEqual(record[8:], hmac.digest(b'\x01'*32,b'CTR1'+struct.pack('>I',1),'sha256')[:24])
            self.assertEqual((root / 'pair/keys.json').stat().st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError):
                p.generate(root / 'image.bin', root / 'g431.bin', root / 'pair')

    def test_reject_invalid_images(self):
        for bad in [b'', b'\xff'*32, bytes(p.MEASURE_LENGTH + 1)]:
            with self.assertRaises(ValueError):
                p.pad_image(bad)
        with self.assertRaises(ValueError):
            p.pad_image(struct.pack('<II', 0x20020000, 0x08000009) + b'G431',
                        p.BOARDS['g431']['image_length'], p.BOARDS['g431']['ram_end'])
        with self.assertRaises(ValueError):
            p.pad_image(bytes(p.BOARDS['g431']['image_length'] + 1),
                        p.BOARDS['g431']['image_length'], p.BOARDS['g431']['ram_end'])
        for value in [0, -1, 0xFFFFFFFF]:
            with self.assertRaises(ValueError):
                p.make_record(bytes(32), value)


if __name__ == '__main__':
    unittest.main()
