cmake_minimum_required(VERSION 3.14)

include(pico_sdk_import.cmake)
project(mg_demo_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_definitions( -DMG_CPU_MAX=2 )
add_executable(demo main.c)

pico_add_extra_outputs(demo)

if(NOT DEFINED ENV{MG_DIR})
    set(ENV{MG_DIR} "..")
endif()

target_include_directories(demo PRIVATE "$ENV{MG_DIR}" "$ENV{MG_DIR}/nvic" )
target_link_libraries(demo pico_stdlib pico_multicore)

