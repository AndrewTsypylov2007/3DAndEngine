cmake_minimum_required(VERSION 3.14)
project(engine_core LANGUAGES CXX)

option(BUILD_TESTS "Build unit tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE CORE_SRC src/*.cpp)
# Exclude main.cpp from library sources (it will be in the executable)
set(FILTERED_SRC)
foreach(f IN LISTS CORE_SRC)
  get_filename_component(fname ${f} NAME)
  if (NOT fname STREQUAL "main.cpp")
	list(APPEND FILTERED_SRC ${f})
  endif()
endforeach()
set(CORE_SRC ${FILTERED_SRC})

# Build core as a static library so tests can link against it
add_library(engine_core_lib STATIC ${CORE_SRC})
target_include_directories(engine_core_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(engine_core src/main.cpp)
target_link_libraries(engine_core PRIVATE engine_core_lib)

if (MSVC)
  target_compile_definitions(engine_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Поддерево плагинов
add_subdirectory(plugins/sample_plugin)

# Найти зависимости через vcpkg / find_package
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(engine_core_lib PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json fmt::fmt)

# Enable testing
include(CTest)
enable_testing()

# Unit tests
if (BUILD_TESTS)
  file(GLOB_RECURSE TEST_SRC tests/*.cpp)
  if (TEST_SRC)
	add_executable(unit_tests ${TEST_SRC})
	# Compile selected core sources directly into tests to avoid linking duplicate mains
	target_sources(unit_tests PRIVATE
	  ${CMAKE_SOURCE_DIR}/src/core/ServiceManager.cpp
	  ${CMAKE_SOURCE_DIR}/src/core/SemVer.cpp
	)
	target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
	target_compile_definitions(unit_tests PRIVATE UNIT_TESTS_MAIN=1)
	add_test(NAME unit_tests COMMAND unit_tests)
  endif()
endif()
