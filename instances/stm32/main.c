/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#include "auth_transport.h"
#include "board.h"
#include "prover.h"
#include "verifier.h"

int main(void) {
    if (!board_init())
        board_fatal(board_error_code);
    auth_platform_t platform = board_platform();
    auth_link_t link = board_link();
    auth_receiver_t receiver = {0};
    uint8_t payload[SIMPLE_REQUEST_LEN];
    uint8_t type;
    uint8_t byte;
    size_t length;
#if AUTH_ROLE_PROVER
    const attestation_range_t ranges[] = {
        {(const uint8_t *)AUTH_MEASURE_START, AUTH_MEASURE_LENGTH}};
    static prover_t prover;
    if (!prover_init(&prover, &platform, ranges, 1))
        board_fatal(board_error_code);
#else
    static verifier_t verifier;
    if (!init_verifier(&verifier, &platform, AUTH_TIMEOUT_MS))
        board_fatal(board_error_code);
    uint32_t last_session = HAL_GetTick() - (AUTH_INTERVAL_MS - 2000u);
#endif
    for (;;) {
#if !AUTH_ROLE_PROVER
        if (verifier_poll(&verifier) == VERIFY_TIMEOUT) {
            board_last_elapsed_ms = (uint32_t)(HAL_GetTick() - verifier.started_ms);
            last_session = HAL_GetTick();
        }
        if (!verifier.outstanding && auth_expired(HAL_GetTick(), last_session, AUTH_INTERVAL_MS)) {
            if (!verifier_challenge(&verifier, payload, sizeof(payload)))
                board_fatal(board_error_code);
            if (!auth_frame_send(&link, AUTH_FRAME_REQUEST, payload, SIMPLE_REQUEST_LEN, 100)) {
                verifier_cancel(&verifier);
                board_report_error(NULL, AUTH_ERROR_UART_TX);
            }
            last_session = HAL_GetTick();
        }
#endif
        int received = link.read(link.user, &byte);
        if (received < 0) {
            receiver.discard = 1;
            board_report_error(NULL, AUTH_ERROR_UART_RX);
            continue;
        }
        if (!received)
            continue;
        int framed = auth_frame_feed(&receiver, byte, HAL_GetTick(), AUTH_FRAME_TIMEOUT_MS, &type,
                                     payload, sizeof(payload), &length);
        if (framed < 0) {
            board_report_error(NULL, AUTH_ERROR_FRAME);
            continue;
        }
        if (!framed)
            continue;
        if (type == AUTH_FRAME_DIAGNOSTIC) {
#if AUTH_ROLE_PROVER
            auth_error_t peer = (auth_error_t)payload[0];
            if (peer > AUTH_ERROR_NONE && peer <= AUTH_ERROR_FLASH_SEQUENCE)
                board_peer_error(peer);
            else
                board_report_error(NULL, AUTH_ERROR_FRAME);
#else
            board_report_error(NULL, AUTH_ERROR_FRAME_TYPE);
#endif
            continue;
        }
#if AUTH_ROLE_PROVER
        if (type != AUTH_FRAME_REQUEST) {
            board_report_error(NULL, AUTH_ERROR_FRAME_TYPE);
            continue;
        }
        uint8_t response[SIMPLE_REPORT_LEN];
        uint32_t started = HAL_GetTick();
        size_t n = prover_handle_request(&prover, payload, length, response, sizeof(response));
        board_last_elapsed_ms = (uint32_t)(HAL_GetTick() - started);
        if (!prover.ready)
            board_fatal(board_error_code);
        if (n && !auth_frame_send(&link, AUTH_FRAME_RESPONSE, response, n, 100))
            board_report_error(NULL, AUTH_ERROR_UART_TX);
#else
        if (type != AUTH_FRAME_RESPONSE) {
            board_report_error(NULL, AUTH_ERROR_FRAME_TYPE);
            continue;
        }
        verify_result_t result = verify(&verifier, payload, length);
        if (result == VERIFY_PASS || result == VERIFY_MISMATCH || result == VERIFY_TIMEOUT) {
            board_last_elapsed_ms = (uint32_t)(HAL_GetTick() - verifier.started_ms);
            last_session = HAL_GetTick();
        }
#endif
    }
}
