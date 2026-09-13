#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, default=root / '.deps')
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    for name, (url, revision) in json.loads((root / 'tools/dependencies.json').read_text()).items():
        dest = args.directory / name
        if not dest.exists():
            subprocess.run(['git', 'init', str(dest)], check=True)
            subprocess.run(['git', '-C', str(dest), 'remote', 'add', 'origin', url], check=True)
        current = subprocess.run(['git', '-C', str(dest), 'rev-parse', 'HEAD'], capture_output=True, text=True)
        if current.returncode == 0 and current.stdout.strip() == revision:
            continue
        dirty = subprocess.check_output(['git', '-C', str(dest), 'status', '--porcelain'], text=True)
        if dirty:
            raise SystemExit(f'Refusing to change modified dependency: {dest}')
        subprocess.run(['git', '-C', str(dest), 'fetch', '--depth', '1', 'origin', revision], check=True)
        subprocess.run(['git', '-C', str(dest), 'checkout', '--detach', revision], check=True)
    print(f'Dependencies ready in {args.directory}')


if __name__ == '__main__':
    main()
