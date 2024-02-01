cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(voyages-and-verve)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Directories
set(SRC_DIR sauce)

# Recursively find all .cpp and .h files in the src directory
file(GLOB_RECURSE SOURCES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.h")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE vendor/SDL/include)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
set(SDL_TEST OFF)
add_subdirectory(vendor/SDL)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2-static)
