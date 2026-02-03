cmake_minimum_required(VERSION 4.2)

project(autocereal LANGUAGES CXX)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

OPTION(BUILD_TESTS "Build autocereal unit tests" ON)

find_package(cereal CONFIG REQUIRED)

add_library(autocereal INTERFACE)
add_library(FR::autocereal ALIAS autocereal)

target_include_directories(autocereal INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(autocereal INTERFACE cereal::cereal)

target_compile_features(autocereal INTERFACE cxx_std_26)

if (BUILD_TESTS)
  add_subdirectory(test)
endif()
