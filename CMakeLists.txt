cmake_minimum_required(VERSION 3.21)
include(cmake/instance.cmake)
project(simple_attestation LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(tinycrypt_lib STATIC
    third_party/tinycrypt/lib/source/sha256.c
    third_party/tinycrypt/lib/source/hmac.c
    third_party/tinycrypt/lib/source/utils.c)
target_include_directories(tinycrypt_lib PUBLIC third_party/tinycrypt/lib/include)
add_library(auth_core STATIC
    core/protocol/src/simple.c core/transport/src/auth_transport.c
    core/platform/src/auth_journal.c
    core/utils/src/crypto_sha256.c core/utils/src/crypto_hmac.c
    core/utils/src/attestation.c
    apps/prover/src/prover.c apps/verifier/src/verifier.c)
target_include_directories(auth_core PUBLIC
    core/protocol/include core/platform/include core/transport/include
    core/utils/include apps/prover/include apps/verifier/include)
target_link_libraries(auth_core PUBLIC tinycrypt_lib)
target_compile_options(auth_core PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
if(INSTANCE STREQUAL "posix")
    include(CTest)
    if(NOT UNIX)
        message(FATAL_ERROR "The posix instance requires a UNIX host")
    endif()
    add_subdirectory(instances/posix)
else()
    add_subdirectory(drivers/stm32)
    include(instances/stm32/CMakeLists.txt)
endif()
