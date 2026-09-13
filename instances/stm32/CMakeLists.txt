set(CMSIS_DIR "${AUTH_DEPS_ROOT}/cmsis-${FAMILY}")
if(NOT DEFINED STARTUP_SOURCE)
    if(NOT DEFINED STARTUP_BASENAME)
        set(STARTUP_BASENAME startup_${DEVICE_LOWER}.s)
    endif()
    set(STARTUP_SOURCE ${CMSIS_DIR}/Source/Templates/gcc/${STARTUP_BASENAME})
endif()
if(NOT EXISTS "${STARTUP_SOURCE}")
    message(FATAL_ERROR "Missing startup source: ${STARTUP_SOURCE}")
endif()
add_executable(firmware ${STARTUP_SOURCE} ${CMAKE_CURRENT_LIST_DIR}/main.c)
target_link_libraries(firmware PRIVATE auth_drivers)
target_compile_options(firmware PRIVATE -Wall -Wextra -Werror)
target_link_options(firmware PRIVATE -T${INSTANCE_DIR}/flash.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/firmware.map -Wl,--print-memory-usage)
set_target_properties(firmware PROPERTIES SUFFIX .elf LINK_DEPENDS ${INSTANCE_DIR}/flash.ld)
add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware> firmware.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:firmware> firmware.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>)
