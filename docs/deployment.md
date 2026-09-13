<!-- Generative AI disclosure: Drafted with Microsoft Copilot; revised with OpenAI Codex (SOL model). -->

# STM32 instance deployment

## Equipment and software

Use WeAct STM32H743VIT6 (2 MiB internal flash, onboard 0.96-inch TFT) and
STM32G431CBU6 (128 KiB, UFQFPN48). These are the confirmed board variants.
Use an ST-LINK probe with SWDIO, SWCLK, GND, target-voltage reference, and NRST
connected to the board. Both boards can be programmed sequentially with one
probe; two probe serial numbers are used below to make target selection explicit.

Install Arm GNU `arm-none-eabi` tools, CMake, Ninja, Python, Git, and
[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html).
Arm GNU 16.2.0 was used for the latest cross-build checks. HAL/CMSIS revisions are
pinned in `tools/dependencies.json`; their upstream licenses remain in the
fetched repositories. TinyCrypt is an existing pinned submodule.

Run the build and provisioning commands in the main README. Use fresh build
folders if changing the MCU or toolchain. The H743 uses its 64 MHz HSI. The
G431 uses the board's 8 MHz HSE through PLLM=2, PLLN=85, and PLLR=2 for 170 MHz.
Both instances use HSI48 for RNG. The timer and UART derive their clocks from
these configurations.

The G431 build uses ST's pinned `startup_stm32g431xx.s`. Its RNG driver accesses
the G431 CR/SR/DR layout through the matching ST HAL, enables clock-error
detection, and rejects every HAL generation failure. No enhanced-RNG or
dual-bank flash setting is used.

## Wiring

| H743 prover | G431 verifier |
|---|---|
| PA2, USART2 TX | PA3, USART2 RX |
| PA3, USART2 RX | PA2, USART2 TX |
| GND | GND |

Use short 3.3 V UART wires at 115200 baud, 8 data bits, no parity, one stop bit,
no hardware flow control. Do not connect TX to TX. The protocol does not use USB
CDC. Avoid connecting two independent power supplies together through 3V3/5V pins.

The H7 TFT uses SPI4: PE12 SCK, PE14 MOSI, PE11 CS, PE13 D/C, and PE10 backlight.
The backlight P-channel MOSFET is active-low. The panel is HannStar/ST7735,
160x80 landscape, with address offsets x=1, y=26. Reset is handled through the
panel's board reset connection plus a software reset. G431 has no LCD; its PC6
user LED indicates successful verification. H7's user LED is PE3.

Pin and panel settings were checked against the official WeAct examples and
schematics: [H7 board](https://github.com/WeActStudio/MiniSTM32H7xx/tree/cc7f685348a6defb26d5c83b74c34c8809179a34),
[G431 board](https://github.com/WeActStudio/WeActStudio.STM32G431CoreBoard/tree/0af1943ee21bf7d5e172a2bfc6480d44aa229de3).

## First programming

The following commands apply to fresh, unprotected boards or boards already
prepared for trusted reprovisioning. Do not apply them to a running pair as a
routine firmware update: they erase counters and install new paired material.

List probes and inspect the target before writing:

```sh
STM32_Programmer_CLI -l stlink
STM32_Programmer_CLI -c port=SWD sn=H7_PROBE_SERIAL mode=UR -ob displ
STM32_Programmer_CLI -c port=SWD sn=G431_PROBE_SERIAL mode=UR -ob displ
```

Replace the serial placeholders with the actual probe serial numbers. Confirm
H743VIT6/2048 KiB and G431CBU6/128 KiB. Bank swapping must be disabled on the
H743. Inspect existing protection and boot settings.
Use internal flash at `0x08000000` as the boot target. Set BOOT0 for normal flash
boot. On the H743 this corresponds to BOOT_CM7_ADD0=0x0800.

Program and verify the **paired HEX files**, which include keys and journal seeds:

```sh
STM32_Programmer_CLI -c port=SWD sn=H7_PROBE_SERIAL mode=UR \
  -e all -w provisioning/pair-01/h743.hex -v
STM32_Programmer_CLI -c port=SWD sn=G431_PROBE_SERIAL mode=UR \
  -e all -w provisioning/pair-01/g431.hex -v
```

Production firmware will not start sessions until protection is configured.
Verify programming before setting RDP, because debug readback becomes restricted.

## Production protection

Apply and read back these settings with STM32CubeProgrammer's Option Bytes view:

| Setting | H743 | G431 |
|---|---|---|
| Read protection | RDP level 1 | RDP level 1 |
| Code WRP | Bank 1 sector 0 | Bank 1 area A pages 0–55 |
| Provisioning WRP | Bank 2 sector 5 | Bank 1 area B page 63 only |
| Journal blocks | Bank 2 sectors 6–7 remain writable | Bank 1 pages 56–57 remain writable |
| Bank mapping | No bank swap | Single bank |

For H743 devices exposing sector flags `nWRP0` through `nWRP15`, code/key
protection corresponds to `nWRP0=0` and `nWRP13=0`; keep `nWRP14=1` and
`nWRP15=1` for the journal. Confirm the names and addressed sectors in `-ob displ`
for your installed programmer version. For G431 the area settings are
`WRP1A_STRT=0`, `WRP1A_END=55`, `WRP1B_STRT=63`, `WRP1B_END=63`.
Unused WRP areas must not overlap the journals. RDP value `0xBB` selects level 1
on these parts. Do not select RDP level 2.

The firmware reads these settings and fails closed if the required protection
or memory geometry is missing. RDP level 1 reduces debug access; returning to
level 0 causes a destructive flash erase on these MCUs. This is why protection
is a deliberate final provisioning step, not an automatic boot action.
Consult [STM32CubeProgrammer's manual](https://www.st.com/resource/en/user_manual/um2237-stm32cubeprogrammer-software-description-stmicroelectronics.pdf)
and the device reference manual before changing option bytes.

Disconnect the debugger and power-cycle both boards after protection is applied.
For a development session with debugger access, build **both** targets in
separate directories with `-DAUTH_REQUIRE_PROTECTION=OFF`, generate a separate
pair from those binaries, and skip the RDP/WRP step. Never reuse a production
reference for a different development image.

## Expected session

1. Both boards validate provisioning, geometry, and persistent journals. The H7
   LCD displays `READY`; a fatal boot error displays `ERROR` if the LCD is usable
   and flashes the user LED.
2. After about two seconds, G431 obtains 16 hardware RNG bytes, increments and
   persists `C_V`, and sends an authenticated challenge containing the approved VS.
3. H7 displays `REQUEST`, rejects stale/bad requests, commits the new counter for
   a valid request, and displays `MEASURING` while HMACing its 128 KiB flash region.
4. H7 displays `PASS` or `MISMATCH` and sends the nonce-bound authenticated result.
5. G431 verifies against its outstanding counter/nonce. Its LED turns on only for
   an authenticated matching report. Timeout, mismatch, or a pending challenge
   leaves the LED off. Sessions repeat after the configured 60-second interval.

The H7's `PASS` is its local measurement result, not confirmation that the G4
received the response. There is no extra acknowledgment in Figure 2. In a
development build, inspect `board_phase` and `board_last_elapsed_ms` over SWD for
G431 diagnostics without adding text to the protocol UART.

## Physical acceptance and negative tests

Use development builds and an isolated pair for destructive fault tests. Keep the
original approved H7 reference unchanged while changing the measured device.
Do not change executable instructions for the simple corruption test: changing
an unused padded flash byte near `0x0801FFF0` makes the mismatch observable
without intentionally breaking program execution. Restore/re-pair after testing.

| Test | Expected outcome |
|---|---|
| Correct paired images | H7 `PASS`; G431 accepts and lights its LED |
| Change an unused measured H7 flash byte without changing G431 VS | Authenticated result 0; G431 reports mismatch |
| Provision one peer with a different K_auth | Requests/replies are rejected; G431 eventually times out |
| Capture and replay a complete request | H7 rejects equal/older counter and sends no report |
| Replay an old response during a new challenge | G431 rejects it; the current challenge remains pending |
| Alter result, counter, VS, nonce, or HMAC | Authentication fails; no authenticated pass |
| Send a truncated or oversized UART frame | Frame is discarded; next delimiter/new frame recovers |
| Disconnect UART or delay response past five seconds | G431 times out and later generates a fresh challenge |
| Reset either board after a completed exchange | Counters resume without reuse; subsequent session succeeds |
| Cut power during journal programming | No false success or silent counter reset; recovery may require maintenance |
| Wrong flash geometry / missing production protection | Boot refuses to start protocol sessions |

A 3.3 V USB-UART adapter and a small host harness can inject frames while the G431
is disconnected from the H7. Never connect two transmitters to the same RX wire.
The host tests already cover these protocol cases without modifying device flash.

## Moving to another H7/G4

Keep `auth_core` and the application state machines unchanged. Add a device directory under `instances/stm32/stm32h7xx/` or
`instances/stm32/stm32g4xx/`, with `CMakeLists.txt`, `board_config.h`, and
`flash.ld`. The leaf selects its family CPU/FPU configuration and CMSIS startup.
The instance name must match its directory; no board-name switch is needed in
the root build. Review UART/SPI pins, clocks, RNG availability, flash programming units,
sector/page geometry, MPU capabilities, cache maintenance, and any second core
or DMA bus master. Replace the storage backend where geometry differs. Update
offline provisioning geometry and measurement length together, then repeat the
host and hardware acceptance tests. Family membership alone does not guarantee
a compatible flash layout or isolation boundary.
