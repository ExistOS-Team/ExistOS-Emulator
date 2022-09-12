cmake_minimum_required(VERSION 3.16)

include(ExternalProject)  

set(CMAKE_TOOLCHAIN_FILE toolchain.cmake)
project(ExistOS-for-HP39gii VERSION 0.1.0)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)


SET(BUILD_SHARED_LIBRARIES OFF)



add_link_options(-T ${CMAKE_SOURCE_DIR}/ldr_ld.script -Wl,--gc-sections)

SET(CMAKE_EXE_LINKER_FLAGS "-static")

include_directories(.)

aux_source_directory(.		LOADER_SRCS)


add_executable(OSLoader.elf ${LOADER_SRCS})

target_link_libraries(OSLoader.elf PRIVATE -lm -lc_nano -nostartfiles)

target_compile_options(OSLoader.elf PRIVATE -march=armv5tej -mcpu=arm926ej-s -mlittle-endian -marm
-Os -pipe -fdata-sections -ffunction-sections -fcommon -finline-functions)

add_custom_command(TARGET OSLoader.elf POST_BUILD
    COMMAND arm-none-eabi-size ${CMAKE_CURRENT_BINARY_DIR}/OSLoader.elf 
    )
	
	
add_custom_target(
  OSLoader.bin ALL
  COMMAND ${CMAKE_OBJCOPY} -I elf32-little -O binary -R .note -R .comment -S ${CMAKE_CURRENT_BINARY_DIR}/OSLoader.elf ${CMAKE_CURRENT_BINARY_DIR}/OSLoader.bin 
  DEPENDS OSLoader.elf 
  )
	