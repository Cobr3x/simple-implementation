<!-- Generative AI disclosure: Drafted with Microsoft Copilot; revised with OpenAI Codex (SOL model). -->

# Startup and synchronization diagnostics

## Interpreting the reported symptom

The application has no normal heartbeat. Older firmware toggles its LED every
200 ms in `board_fatal()`. The G431 diagnostic firmware repeats a 900 ms marker,
the error-code tens digit as 400 ms pulses, and the units digit as 120 ms pulses.
For example, `E22` is marker, two medium pulses, then two short pulses. A fatal
Verifier never reaches its initial challenge and the UART remains silent.

When USART2 initialized before the failure, the G431 also repeats a diagnostic
frame. The H743 displays `VERIFIER` and the exact code, such as `E22 RDP`.
Diagnostic frames cannot start attestation, change a counter, or produce an
authenticated result. Failures before UART initialization remain LED-only.

On the H743, a visible error proves that the system clock and LCD initialized.
The most likely startup failures are then:

1. `E05 PROTECT`: the production image requires RDP level 1 and the documented
   code/key write protection, but the option bytes do not match.
2. `E06 PROVISION`: the provisioned record is absent, corrupt, for the wrong
   role, or has incompatible measurement geometry. This commonly means the raw
   build image was flashed instead of the complete segmented release or `h743.hex`.
3. `E07 JOURNAL`: neither authenticated counter journal contains a valid seed,
   or a journal contains a torn, unordered, or incorrectly keyed record.

`READY` and `Cxxxxxxxx` confirm that protection, provisioning metadata, keys,
reference state integrity, and an authenticated counter were loaded. The
counter is hexadecimal. This does not prove that the peer has the same keys;
that is established when request authentication succeeds.

The G431 status LED turns on while a challenge is outstanding. Normally this
happens about two seconds after verifier boot and lasts until a valid response
or the five-second timeout. If the G431 instead enters the continuous fatal
blink, it failed board initialization, protected-state loading, RNG, challenge
construction, or counter persistence and sends no challenge.

## H743 display identifiers

| ID | Meaning |
|---|---|
| `E01 CLOCK` | System/RNG clock configuration failed before normal startup; the LCD may not yet be available. |
| `E02 UART INIT` | USART2 initialization failed. |
| `E03 RNG` | RNG initialization or challenge nonce generation failed. |
| `E04 LCD` | LCD initialization failed; this code may not be visible if the display itself is unusable. |
| `E05 PROTECT` | Protection validation failed without a more specific code (older firmware). |
| `E06 PROVISION` | Provisioning magic, version, role, geometry, counter, or record digest is invalid. |
| `E07 JOURNAL` | The authenticated persistent counter journal could not be loaded. |
| `E08 UART RX` | USART overrun, framing, noise, or parity error occurred. |
| `E09 FRAME` | A COBS frame was malformed, oversized, or timed out before its delimiter. |
| `E10 TYPE` | A valid frame carried the wrong message type for this role. |
| `E11 LENGTH` | A request reached the prover with an invalid decoded length. |
| `E12 STALE` | The request counter was equal to or below the prover counter. |
| `E13 AUTH` | Request HMAC verification failed. Check pairing and frame integrity. |
| `E14 COUNTER` | The next authenticated counter could not be persisted. |
| `E15 MEASURE` | HMAC measurement of the approved flash range failed. |
| `E16 RESPONSE` | Response authentication or serialization failed. |
| `E17 CHALLENGE` | The verifier could not construct another challenge or exhausted its counter. |
| `E18 UART TX` | A complete request or response could not be transmitted. |
| `E19 INTERNAL` | An invalid internal state reached the fatal handler. |
| `E20 FLASH SIZE` | The MCU-reported flash capacity does not match the selected instance. |
| `E21 BANK MAP` | Flash bank swapping/remapping or the required G4 dual-bank mode is wrong. |
| `E22 RDP` | The production image requires RDP level 1, but it is not active. |
| `E23 CODE WRP` | The production code sector/page range is not write-protected. |
| `E24 KEY WRP` | The provisioning sector/page range is not write-protected. |
| `E25 FLASH OPEN` | The flash controller could not be unlocked for a counter update. |
| `E26 FLASH WRP` | Hardware write protection blocked the counter journal update. |
| `E27 FLASH ALIGN` | The journal write violated the MCU flash alignment or programming sequence. |
| `E28 FLASH WRITE` | The flash controller rejected the counter write for another reason. |
| `E29 FLASH CHECK` | Data read after programming did not match the counter record. |
| `E30 FLASH ERASE` | Rotation to the other journal block could not erase its page/sector. |
| `E31 WRITE SIZE` | The G4 rejected the flash programming width. |
| `E32 FLASH SEQ` | The G4 rejected the flash programming sequence. |

`E27` immediately after provisioning can mean a dense, gap-filled BIN programmed
the journal's apparent `0xFF` slots and their ECC bits. Rebuild and flash with the
segmented deployment workflow; a readback of `0xFF` alone does not prove that an
ECC flash double-word remains erased.

Firmware that only reports `E05 PROTECT` cannot distinguish these option-byte
conditions. Flash the paired development images and read the replacement code.
Development images skip the RDP and WRP requirements, so only `E20` or `E21`
can stop them at this stage. Production images additionally enforce `E22`--`E24`.

Challenge-validation errors are recoverable: the H743 keeps waiting and a later
valid request replaces the error with `REQUEST`, `MEASURING`, and the final
result. Counter-save failures are fatal because continuing could reuse state.

## Authentication flow

1. Both devices validate their platform and load their protected record and
   authenticated journal. The H743 waits in `READY`; it never initiates traffic.
2. About two seconds after boot, the G431 obtains a 16-byte hardware RNG nonce,
   increments its counter, builds `C_V || VS || nonce`, computes the request
   HMAC, and persists `C_V` before transmission. It records that counter and
   nonce as the only outstanding challenge.
3. The H743 accepts only a request of the exact framed length whose counter is
   greater than `C_P` and whose HMAC is valid. It persists the received counter
   before measurement.
4. The accepted request changes the display to `MEASURING`. The H743 computes
   `HMAC(K_attest, approved flash)` and compares it with the request's `VS`.
5. The H743 sends the result with an HMAC over `result || C_P || nonce`. The
   counter and nonce therefore bind the response to the outstanding challenge.
6. The G431 rejects a response if no challenge is pending, its deadline has
   expired, its length/result is invalid, or its HMAC does not match the saved
   counter and nonce. A valid result selects `PASS` or `MISMATCH`.

## Power-up behavior

Neither device is required to power up first. Powering the H743 first is more
convenient because it waits indefinitely for a valid request. If the G431 sends
its first challenge while the H743 is off, it times out after five seconds and
waits 60 seconds before issuing another challenge. Resetting the G431 requests a
new challenge sooner, at the cost of consuming another persisted counter.

UART idle never changes the H743 from `READY` to an error. A partial frame is
marked invalid only when later traffic or a delimiter lets the receiver observe
the frame timeout. Electrical UART faults produce `E08`; malformed framing
produces `E09`.

The counters normally start from the same provisioned seed, but exact equality
at every boot is unnecessary. The G431 commits before sending, so a lost request
can leave it ahead; the next higher authenticated counter lets the H743 catch
up. A G431 counter equal to or behind the H743 is rejected as stale. If that
state persists, re-provision both devices together with new keys or with an
initial counter above both deployed high-water marks.

## First checks on the boards

1. Flash the paired artifacts from the same `out/RELEASE` directory, rather than
   either raw `firmware.bin`.
2. For initial debugger testing, build and provision both devices with
   `./deploy.sh build --development`; production builds require the documented
   RDP/WRP option bytes.
3. Connect H743 PA2 to G431 PA3, H743 PA3 to G431 PA2, and share ground.
4. Power the H743 and confirm `READY Cxxxxxxxx`, then power or reset the G431.
5. Around two seconds later, confirm that the G431 LED enters its outstanding
   challenge indication and that the H743 changes to `REQUEST`.

If the H743 remains `READY`, it has not decoded a request. If the G431 never
indicates an outstanding challenge, diagnose its initialization/provisioning.
If the G431 indicates a challenge but the H743 remains `READY`, check crossed
UART wiring, common ground, 3.3 V levels, and that both images use USART2 at
115200 baud, 8N1.
