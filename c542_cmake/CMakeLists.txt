# file-format: 1.0.0
cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

include(cmake/target.cmake)
include(cmake/${TOOLCHAIN_FILE})

project(c542)
add_executable(c542)
include(cmake/files.cmake)
include(cmake/flags.cmake)
include(cmake/components.cmake)

# Stubs to be able to add compiler flag overrides, e.g. -Os
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  app_board.c
  app_console.c
  letter_shell/shell.c
  letter_shell/shell_ext.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
  letter_shell
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
  SHELL_CFG_USER="shell_config.h"
)

target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional compiler flags here
)

target_link_options(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker flags here
)

target_link_directories(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker search directories here
)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker libraries here
)

add_custom_command(TARGET c542 POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:c542> $<TARGET_FILE_DIR:c542>/c542.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:c542>
  VERBATIM
)
