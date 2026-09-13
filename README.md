# SIMPLE-style STM32 attestation

Two STM32 devices perform authenticated remote attestation over UART.

| Device | Role |
| --- | --- |
| STM32H743VIT6 with ST7735 display | Prover |
| STM32G431CBU6 | Verifier |

## Prerequisites

Ubuntu and Debian:

```sh
sudo apt update
sudo apt install git cmake ninja-build python3 python3-venv gcc-arm-none-eabi dfu-util
```

## Build

```sh
./deploy.sh build
```

The script installs pinned dependencies, runs the tests, builds both firmware images, and creates a
paired provisioned release.

The generated files are in `out/latest/`. They contain secret keys and must not be shared or
committed.

If the ARM toolchain is installed in another directory:

```sh
ARM_TOOLCHAIN_PREFIX=/opt/arm-gnu/bin/arm-none-eabi ./deploy.sh build
```

The default build works with development option bytes. Use `./deploy.sh build --production` only
after configuring the required RDP and WRP option bytes.

## Flash

Put the H743 in USB DFU mode, connect it, and run:

```sh
./deploy.sh flash h7
```

Disconnect it, put the G431 in USB DFU mode, and run:

```sh
./deploy.sh flash g431
```

These commands flash the firmware, provisioning record, and two counter-journal seeds from
`out/latest/`. Flash both devices from the same release.

To flash a specific release:

```sh
./deploy.sh flash h7 --release out/RELEASE_NAME
./deploy.sh flash g431 --release out/RELEASE_NAME
```

## Run

Return both boards to normal flash boot and connect:

| H743 | G431 |
| --- | --- |
| PA2 (TX) | PA3 (RX) |
| PA3 (RX) | PA2 (TX) |
| GND | GND |

The UART connection uses 3.3 V logic at 115200 baud, 8N1. Either board may be powered first. The
prover remains ready until it receives a valid challenge.

## Debug

The H743 display reports errors as `E<number> NAME`. The G431 sends the same identifiers to the
prover and signals failures with its LED pattern.

| Error | Meaning |
| --- | --- |
| `E13 REQUEST AUTH` | The devices have different authentication keys. |
| `E22 RDP` | A production build requires RDP Level 1, but it is not configured. |
| `E27 FLASH ALIGN` | A counter-journal location was not physically erased. |

See [docs/troubleshooting.md](docs/troubleshooting.md) for the complete error scheme and recovery
steps.
