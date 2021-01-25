# cmake_minimum_required(VERSION <specify CMake version here>)
project(water_pump_on_mega)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(water_pump_on_mega
        water-pump-on-mega.ino Data.h credentials.h)
