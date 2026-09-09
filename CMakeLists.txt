cmake_minimum_required(VERSION 3.15)
project(simple_attestation C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_H7 "Build for STM32H7" OFF)
option(BUILD_G4 "Build for STM32G4" OFF)

# TODO: Fix TinyCrypt build integration.
add_subdirectory(third_party/tinycrypt)

add_library(tinycrypt_lib
    third_party/tinycrypt/source/sha256.c
    third_party/tinycrypt/source/hmac.c
    third_party/tinycrypt/source/utils.c
)

target_include_directories(tinycrypt_lib PUBLIC
    third_party/tinycrypt/include
)

# TODO: Fix source and include paths.
add_library(core_utilities
    core/simple.c
    core/utilities/crypto_sha256.c
    core/utilities/crypto_hmac.c
    core/utilities/attestation.c
)

target_include_directories(core_utilities PUBLIC
    core
    core/utilities
)

target_link_libraries(core_utilities tinycrypt_lib)

# TODO: Add missing UART, transport, verifier, and board code.
add_library(drivers_uart
    drivers/uart/uart_hw.h
)

target_include_directories(drivers_uart PUBLIC
    drivers/uart
)

add_library(transport_uart
    apps/common/transport_uart.c
)

target_include_directories(transport_uart PUBLIC
    apps/common
)

target_link_libraries(transport_uart drivers_uart)

add_library(app_prover
    apps/prover/prover.c
)

target_include_directories(app_prover PUBLIC
    apps/prover
)

target_link_libraries(app_prover core_utilities transport_uart)

add_library(app_verifier
    apps/verifier/verifier.c
)

target_include_directories(app_verifier PUBLIC
    apps/verifier
)

target_link_libraries(app_verifier core_utilities transport_uart)


if (BUILD_H7)
    add_executable(h7_verifier
        instances/h7_verifier/main.c
        drivers/uart/uart_stm32h7.c
    )
    target_link_libraries(h7_verifier app_verifier)
endif()

if (BUILD_G4)
    add_executable(g4_prover
        instances/g4_prover/main.c
        drivers/uart/uart_stm32g4.c
    )
    target_link_libraries(g4_prover app_prover)
endif()
