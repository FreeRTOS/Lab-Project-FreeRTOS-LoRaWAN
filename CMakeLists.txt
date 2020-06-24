cmake_minimum_required(VERSION 3.13)

project(freertos_lorawan)

set(build_target lorawan_class_a )

set(nrf5_sdk "${CMAKE_SOURCE_DIR}/freertos/vendors/nordic/nRF5_SDK_15.2.0")

set(nrf52840_dir "${CMAKE_SOURCE_DIR}/freertos/vendors/nordic/boards/nrf52840-dk")

add_executable(${build_target} class_a/main.c)


# As of now there's no offical way to redefine config files outside of FreeRTOS source tree.
# This is a temporary approach to inject an include path so that this takes precedence over the
# config file directory inside FreeRTOS.
include_directories(BEFORE freertos-configs)

set(AFR_BOARD nordic.nrf52840-dk CACHE INTERNAL "")


add_subdirectory(freertos)


   target_link_libraries(
       ${build_target}
       PRIVATE
       AFR::kernel
       "${nrf5_sdk}/external/nrf_cc310/lib/libnrf_cc310_0.9.10.a"
       "${AFR_COMPILER_DIR}/../../../lib/libc_v7em_fpv5_d16_hard_t_le_eabi.a"
       "${AFR_COMPILER_DIR}/../../../lib/libdebugio_mempoll_v7em_fpv4_sp_d16_hard_t_le_eabi.a"
       "${AFR_COMPILER_DIR}/../../../lib/libm_v7em_fpv4_sp_d16_hard_t_le_eabi.a"
       "${AFR_COMPILER_DIR}/../../../lib/libc_v7em_fpv4_sp_d16_hard_t_le_eabi.a"
       "${AFR_COMPILER_DIR}/../../../lib/libcpp_v7em_fpv4_sp_d16_hard_t_le_eabi.a"
       "${AFR_COMPILER_DIR}/../../../lib/libdebugio_v7em_fpv4_sp_d16_hard_t_le_eabi.a"
       "${AFR_COMPILER_DIR}/../../../lib/libvfprintf_v7em_fpv4_sp_d16_hard_t_le_eabi.o"
       "${AFR_COMPILER_DIR}/../../../lib/libvfscanf_v7em_fpv4_sp_d16_hard_t_le_eabi.o"
    )



find_program(gcc_objectcopy objcopy HINTS "${AFR_COMPILER_DIR}")
find_program(gcc_size arm-none-eabi-size HINTS "${AFR_COMPILER_DIR}")

if(NOT gcc_objectcopy)
     message(FATAL_ERROR "Cannot find objcopy.")
endif()

if(NOT gcc_size)
    message(FATAL_ERROR "Cannot find arm-none-eabi-size.")
endif()

set( mkld_flags
         -memory-map-segments "FLASH RX 0x0 0x100000$<SEMICOLON>RAM RWX 0x20000000 0x40000"
         -section-placement-file "${nrf52840_dir}/flash_placement.xml"
         -check-segment-overflow
         -symbols "__STACKSIZE__=8192$<SEMICOLON>__STACKSIZE_PROCESS__=0$<SEMICOLON>__HEAPSIZE__=8192"
         -section-placement-macros
         "FLASH_PH_START=0x0$<SEMICOLON>FLASH_PH_SIZE=0x100000$<SEMICOLON>RAM_PH_START=0x20000000$<SEMICOLON>RAM_PH_SIZE=0x40000$<SEMICOLON>FLASH_START=0x27000$<SEMICOLON>FLASH_SIZE=0xda000$<SEMICOLON>RAM_START=0x200046F8$<SEMICOLON>RAM_SIZE=0x3B908"
)


add_custom_command(
    TARGET ${build_target}
    PRE_LINK
    COMMAND VERBATIM "${AFR_COMPILER_DIR}/../../../bin/mkld" ${mkld_flags} "${CMAKE_BINARY_DIR}/aws_demos.ld"
)

set(output_file "$<TARGET_FILE_DIR:${build_target}>/${build_target}.hex")

add_custom_command(
    TARGET ${build_target} POST_BUILD
    COMMAND "${gcc_objectcopy}" -O ihex "$<TARGET_FILE:${build_target}>" "${output_file}"
    COMMAND "${gcc_size}" "$<TARGET_FILE:${build_target}>"
)

add_custom_command(
    TARGET ${build_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${output_file}" "${CMAKE_BINARY_DIR}"
)

