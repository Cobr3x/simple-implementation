#!/usr/bin/env python3
"""Create paired images offline; never programs a device or changes option bytes."""
import argparse
import hashlib
import hmac
import json
import os
from pathlib import Path
import secrets
import struct

MEASURE_START = 0x08000000
MEASURE_LENGTH = 128 * 1024
BOARDS = {
    'h743': {'role': 1, 'keys': 0x081A0000, 'journals': [0x081C0000, 0x081E0000],
             'image_length': 128 * 1024, 'ram_end': 0x20020000},
    'g431': {'role': 2, 'keys': 0x0801F800, 'journals': [0x0801C000, 0x0801C800],
             'image_length': 112 * 1024, 'ram_end': 0x20008000},
}


def pad_image(image, image_length=MEASURE_LENGTH, ram_end=0x20020000):
    if len(image) < 8 or len(image) > image_length:
        raise ValueError(f'Firmware must contain a vector table and fit in {image_length // 1024} KiB')
    sp, reset = struct.unpack_from('<II', image)
    if not 0x20000000 < sp <= ram_end or sp % 8:
        raise ValueError('Invalid initial stack pointer')
    if not reset & 1 or not MEASURE_START <= (reset & ~1) < MEASURE_START + len(image):
        raise ValueError('Reset vector must point into this internal-flash image')
    return image.ljust(image_length, b'\xff')


def make_record(key, counter):
    if not 1 <= counter < 0xFFFFFFFF:
        raise ValueError('Initial counter must be between 1 and 0xfffffffe')
    encoded = struct.pack('>I', counter)
    mac = hmac.digest(key, b'CTR1' + encoded, 'sha256')[:24]
    return encoded + struct.pack('>I', counter ^ 0xFFFFFFFF) + mac


def make_material(role, k_auth, k_attest, reference, counter):
    body = b'SIMPLE01' + struct.pack('>II', 1, role) + k_auth + k_attest + reference
    body += struct.pack('>III', MEASURE_START, MEASURE_LENGTH, counter)
    assert len(body) == 124
    return (body + hashlib.sha256(body).digest()).ljust(160, b'\xff')


def ihex_record(address, kind, data):
    raw = bytes([len(data)]) + struct.pack('>H', address) + bytes([kind]) + data
    return ':' + (raw + bytes([(-sum(raw)) & 255])).hex().upper() + '\n'


def ihex(segments):
    result, upper = [], None
    previous_end = 0
    for address, data in sorted(segments):
        if address < previous_end:
            raise ValueError('Overlapping image segments')
        previous_end = address + len(data)
        for offset in range(0, len(data), 16):
            current = address + offset
            if current >> 16 != upper:
                upper = current >> 16
                result.append(ihex_record(0, 4, struct.pack('>H', upper)))
            result.append(ihex_record(current & 65535, 0, data[offset:offset + 16]))
    result.append(ihex_record(0, 1, b''))
    return ''.join(result)


def write_private(path, data):
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as output:
        output.write(data)


def generate(h7, g431, out, keys=None, counter=1):
    inputs = {'h743': h7, 'g431': g431}
    raw_images = {name: path.read_bytes() for name, path in inputs.items()}
    images = {
        name: pad_image(raw_images[name], config['image_length'], config['ram_end'])
        for name, config in BOARDS.items()
    }
    if keys is None:
        k_auth, k_attest = secrets.token_bytes(32), secrets.token_bytes(32)
    else:
        value = json.loads(keys.read_text())
        k_auth, k_attest = bytes.fromhex(value['k_auth']), bytes.fromhex(value['k_attest'])
    if len(k_auth) != 32 or len(k_attest) != 32 or k_auth == k_attest:
        raise ValueError('Provide two distinct 32-byte keys')
    seed = make_record(k_auth, counter)
    reference = hmac.digest(k_attest, images['h743'], 'sha256')
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    write_private(out / 'keys.json', (json.dumps({'k_auth': k_auth.hex(), 'k_attest': k_attest.hex()}, indent=2) + '\n').encode())
    manifest = {'measurement_address': hex(MEASURE_START), 'measurement_length': MEASURE_LENGTH,
                'initial_counter': counter, 'reference_vs': reference.hex(), 'images': {}}
    for board, config in BOARDS.items():
        material = make_material(config['role'], k_auth, k_attest, reference, counter)
        segments = [(MEASURE_START, raw_images[board]), (config['keys'], material)]
        segments.extend((address, seed) for address in config['journals'])
        write_private(out / f'{board}.hex', ihex(segments).encode())
        write_private(out / f'{board}-provision.bin', material)
        write_private(out / f'{board}-journal-a.bin', seed)
        write_private(out / f'{board}-journal-b.bin', seed)
        suffix = 'measured' if board == 'h743' else 'firmware-region'
        write_private(out / f'{board}-{suffix}.bin', images[board])
        manifest['images'][board] = {'sha256': hashlib.sha256(images[board]).hexdigest(),
                                     'key_address': hex(config['keys']),
                                     'journal_addresses': list(map(hex, config['journals']))}
    write_private(out / 'manifest.json', (json.dumps(manifest, indent=2) + '\n').encode())
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--h7', required=True, type=Path)
    parser.add_argument('--g431', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--keys', type=Path, help='Existing offline key file; omit for fresh pairing')
    parser.add_argument('--counter', type=lambda s: int(s, 0), default=1)
    args = parser.parse_args()
    try:
        manifest = generate(args.h7, args.g431, args.out, args.keys, args.counter)
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'Provisioning failed: {error}\n')
    print(f'Paired images written to {args.out}; reference VS: {manifest["reference_vs"]}')
    print('HEX files contain secrets. Protect the output directory; do not commit it.')


if __name__ == '__main__':
    main()
