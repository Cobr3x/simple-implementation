<!-- Generative AI disclosure: Drafted with Microsoft Copilot; revised with OpenAI Codex (SOL model). -->

# Protocol and platform contracts

## Request: 84 bytes

| Offset | Length | Field |
|---|---:|---|
| 0 | 4 | `C_V`, unsigned big-endian |
| 4 | 32 | Expected valid state `VS` |
| 36 | 16 | Hardware RNG nonce |
| 52 | 32 | `HMAC-SHA256(K_auth, request[0:52])` |

## Response: 33 bytes

| Offset | Length | Field |
|---|---:|---|
| 0 | 1 | Result, exactly 0 or 1 |
| 1 | 32 | `HMAC-SHA256(K_auth, result || BE32(C_P) || nonce)` |

The response follows Figure 2 and does not transmit a counter. The verifier uses
its single outstanding counter and nonce. HMACs and state values are full-length
32-byte outputs; no C structure layout is part of the wire format.

The prover accepts only requests with `C_V > C_P` and a valid HMAC. It commits
`C_P = C_V` to persistent storage before producing any response. It then computes
`VS' = HMAC-SHA256(K_attest, range_0 || ... || range_n)`, compares it with `VS`,
and authenticates the result with the received nonce. Request authentication,
counter commit, measurement, and response computation run inside the platform's
atomic section. Temporary cryptographic state is erased before leaving it.

The verifier generates fresh nonce bytes, computes its next request, and commits
its counter before making the request available for transmission. An RNG or
storage failure produces no usable challenge. A counter at `UINT32_MAX` cannot
advance; authenticated maintenance is required. There is no counter wraparound.

The verifier accepts a valid response only before its deadline and only once.
Malformed or unauthenticated replies leave the outstanding challenge pending
until timeout, so injected noise does not erase a legitimate challenge. At the
deadline the nonce is discarded. A lost response is handled with a new nonce and
higher counter on the next session, never by replaying a stale request.

## UART framing

Each frame is COBS encoded and terminated by a zero byte:

```
COBS(type[1] || payload_length[1] || payload) || 0x00
```

Type 1 contains exactly 84 request bytes; type 2 contains exactly 33 response
bytes. A sender also transmits a leading zero to recover a receiver left in a
partial frame. The maximum frame size is 88 bytes without that leading zero.
No CRC is needed for acceptance: payload integrity is checked with HMAC. Frame
headers only select one of the two fixed payload formats.

Decoding uses fixed buffers. Invalid lengths, COBS overruns, buffer overflow,
UART errors, or a 250 ms frame deadline discard the frame. Recovery occurs at the
next delimiter. No received data controls allocation sizes or memory addresses.
The measured flash range comes from board configuration, not the network.

## Time and scheduling

STM32 firmware uses free-running 32-bit TIM2 at 1 kHz. `HAL_GetTick()` reads that
counter after initialization, so flash/SPI timeouts continue even when interrupts
are masked. Elapsed-time subtraction supports timer wraparound. Configured
deadlines must be positive and no more than `INT32_MAX` milliseconds; service
the verifier regularly, not after an entire timer cycle has elapsed.

The initial verifier challenge starts about two seconds after boot. Later
sessions start 60 seconds after the previous session completes or times out.
`AUTH_INTERVAL_MS` and `AUTH_TIMEOUT_MS` are board settings. The default response
timeout is five seconds, including transmission, H7 UI/processing, and response
reception. This is an operational deadline, not a distance-bounding proof.
`board_last_elapsed_ms` records prover processing or verifier exchange time for
debugger inspection in development mode.

## Porting

`auth_platform_t` supplies load/commit, RNG, clock, atomic-entry/exit, and optional
phase-display callbacks. `auth_link_t` supplies a nonblocking byte read and a
bounded complete-buffer write. The current board backend uses UART; an SPI link
can implement the same stream contract after defining master scheduling and
chip-select framing. SPI4 on the H7 is currently dedicated to the LCD.

`prover_t` and `verifier_t` are single-owner contexts. Do not call one context
concurrently or re-enter it through a callback. Callbacks must not modify the
request, context, or configured ranges while they are in use. Range pointers
must be valid immutable memory. Keep UI callbacks bounded; on a port with masked
interrupts, do not use interrupt-dependent transfers inside the atomic section.
