cmake_minimum_required(VERSION 3.20)

# ==============================================================================
# 3DAndEngine Core v0.4.0 "Pure Blind Motherboard Architecture"
# ==============================================================================
project(3DAndEngineCore 
    VERSION 0.4.0 
    DESCRIPTION "Ultra-High Performance Lock-Free Modular Game Engine Core"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# Бинарники и рантайм-папки (plugins/shaders/assets/configs) собираются внутри каталога сборки: <build>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Конфигурация для мульти-конфигурационных генераторов (Visual Studio, Xcode)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# ==============================================================================
# ФЛАГИ КОМПИЛЯЦИИ И ОПТИМИЗАЦИИ (AAA Standard C++20)
# ==============================================================================
if(MSVC)
    # /W4 - строгие предупреждения
    # /WX - трактовать предупреждения как ошибки
    # /permissive- - строгое соответствие стандарту C++20
    # /Zc:preprocessor - стандартный C++20 препроцессор
    # /wd4201 - анонимные структуры/объединения
    # /wd4189 - инициализированные, но неиспользуемые переменные (в тестах)
    # /wd4505 - неиспользуемые статические inline функции
    # /wd4324 - структура дополнена байтами выравнивания (alignas(64) для кэш-линий)
    add_compile_options(
        /W4 /WX /permissive- /Zc:preprocessor
        /wd4201 /wd4189 /wd4505 /wd4324
    )
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else()
    # GCC / Clang флаги
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror 
        -Wno-unused-variable -Wno-unused-parameter
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native)
    endif()
endif()

# ==============================================================================
# 1. ЗАГОЛОВОЧНЫЙ SDK ДВИЖКА (3DAndEngineCore Interface Target)
# ==============================================================================
add_library(3DAndEngineCore INTERFACE)

target_include_directories(3DAndEngineCore INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Системная линковка потоков и динамической загрузки библиотек (dlopen / dlsym)
find_package(Threads REQUIRED)
target_link_libraries(3DAndEngineCore INTERFACE Threads::Threads)

if(NOT WIN32)
    target_link_libraries(3DAndEngineCore INTERFACE ${CMAKE_DL_LIBS})
endif()

# ==============================================================================
# 2. ПОСТ-БИЛД: АВТОМАТИЧЕСКОЕ СОЗДАНИЕ ДЕРЕВА ПАПОК SDK
# ==============================================================================
# Создаем папки для "слепой" загрузки плагинов (.dll / .so), шейдеров и ресурсов
add_custom_target(EngineSDKDirectories ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/configs"
    COMMENT "[3DAndEngine SDK] Initializing runtime directory structure (bin/plugins, bin/shaders, bin/assets)"
)

# ==============================================================================
# 3. ОСНОВНОЙ РАНТАЙМ ДВИЖКА (Engine Runtime Executable)
# ==============================================================================
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Если есть файлы в src/, собираем основной исполняемый файл
if(ENGINE_SOURCES)
    add_executable(3DAndEngine ${ENGINE_SOURCES})
    target_link_libraries(3DAndEngine PRIVATE 3DAndEngineCore)
    add_dependencies(3DAndEngine EngineSDKDirectories)
endif()

# ==============================================================================
# 4. ТЕСТЫ (встроенное самотестирование ядра)
# ==============================================================================
# Юнит-тесты находятся в src/UnitTestsMain.cpp и входят в исполняемый файл движка.
# Запуск с флагом --ci выполняет self-test и 30 headless-кадров.
if(TARGET 3DAndEngine)
    add_test(NAME CoreSelfTestAndHeadlessRun COMMAND 3DAndEngine --ci)
endif()

# ==============================================================================
# СТАТУС КОНФИГУРАЦИИ
# ==============================================================================
message(STATUS "=======================================================")
message(STATUS " 3DAndEngine Core v${PROJECT_VERSION} successfully configured")
message(STATUS " Standard:    C++${CMAKE_CXX_STANDARD} (Strict Conformance)")
message(STATUS " Platform:    ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS " Compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " Bin Output:  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=======================================================")