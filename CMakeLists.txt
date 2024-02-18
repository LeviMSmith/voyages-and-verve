cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(voyages-and-verve)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Directories
set(SRC_DIR sauce)
set(RES_DIR res)
set(TEST_DIR test)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE})

# Recursively find all .cpp and .h files in the src directory
file(GLOB_RECURSE SOURCES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.h")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE vendor/SDL/include)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/spdlog/include)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
set(SDL_TEST OFF)
add_subdirectory(vendor/SDL)

add_subdirectory(vendor/spdlog)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2-static)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog)

# Resource copy
add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/res
          $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)

enable_testing()
add_subdirectory(vendor/googletest)

file(GLOB_RECURSE TEST_SOURCES "${TEST_DIR}/*.cpp")

list(FILTER SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_executable(${PROJECT_NAME}_test ${TEST_SOURCES} ${SOURCES})

target_include_directories(${PROJECT_NAME}_test PRIVATE ${SRC_DIR})
target_include_directories(${PROJECT_NAME}_test PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_include_directories(${PROJECT_NAME}_test PRIVATE vendor/SDL/include)
target_include_directories(${PROJECT_NAME}_test PRIVATE vendor/spdlog/include)

target_link_libraries(${PROJECT_NAME}_test PRIVATE gtest gtest_main)
target_link_libraries(${PROJECT_NAME}_test PRIVATE SDL2-static)
target_link_libraries(${PROJECT_NAME}_test PRIVATE spdlog)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)
