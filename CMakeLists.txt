cmake_minimum_required(VERSION 3.10)

project(gp25_exercises)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(ARCH x64)

# 3rd party libraries
add_subdirectory(lib/SDL)
include_directories(lib/stb)

# repo's executables
add_subdirectory(examples)
add_subdirectory(playground)
# add_subdirectory(live_coding) # uncomment if you want to use the live coding examples
add_subdirectory(exercises)
add_subdirectory(exercises_solutions)