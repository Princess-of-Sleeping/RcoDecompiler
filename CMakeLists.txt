cmake_minimum_required(VERSION 3.0)

project(RcoDecompiler C)

set(CMAKE_C_COMPILE_FEATURES "${CMAKE_C_FLAGS} -Wunused-result -Wl,-q -Wall -O3 -fno-inline -fno-builtin -fshort-wchar")

include_directories(
)

add_executable(${PROJECT_NAME}
  src/main.c
  src/fs_list.c
)

target_link_libraries(${PROJECT_NAME}
  z
)
