cmake_minimum_required(VERSION 3.25)

# Tells Cmake to use clang by default.
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} clang PATHS ENV PATH NO_DEFAULT_PATH)
# find_program(CMAKE_C_COMPILER NAMES $ENV{CC} gcc PATHS ENV PATH NO_DEFAULT_PATH)

project(guessing-game LANGUAGES C)

# I usually enable all clang diagnostics for maximum compiler check except for a 
# few like `unsafe-buffer-use`.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=c17 -Weverything -Werror -Wno-switch-enum -Wno-unused-function -Wno-unsafe-buffer-usage -Wno-disabled-macro-expansion")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c17 -g -Wcast-align")

add_executable(guessing-game-server src/server.c)
add_executable(guessing-game-client src/client.c)