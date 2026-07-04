cmake_minimum_required(VERSION 3.24)
project(rednoise_test_package LANGUAGES C)

find_package(rednoise CONFIG REQUIRED)

add_executable(example src/example.c)
target_link_libraries(example PRIVATE rednoise::rednoise)
