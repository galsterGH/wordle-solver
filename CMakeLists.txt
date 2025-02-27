cmake_minimum_required(VERSION 3.30)
project(Wordle)

set(CMAKE_CXX_STANDARD 20)  # Use C++20 (recommended) or C++11 if needed
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Ensures strict compliance

add_executable(wordle wordle.cpp)
