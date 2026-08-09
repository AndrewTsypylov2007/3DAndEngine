cmake_minimum_required(VERSION 3.20)

# ОБНОВЛЕНО: Версия 0.3.0 "Материнская плата"
project(3DAndEngineCore VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройка путей вывода бинарников (в корень SDK папки bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Глобальные флаги для AAA-разработки
if(MSVC)
    # /wd4201 - безымянные структуры (нужны для векторов/матриц)
    # /wd4189 - неиспользуемые переменные (полезно в тестах)
    # /wd4505 - неиспользуемые статические функции (из LibraryLoader/null_subsystems)
    add_compile_options(/W4 /WX /permissive- /wd4201 /wd4189 /wd4505)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-variable)
endif()

# ==============================================================================
# 1. ГЛАВНОЕ ЯДРО (SDK INTERFACE)
# ==============================================================================
add_library(3DAndEngineCore INTERFACE)
target_include_directories(3DAndEngineCore INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Для Linux/macOS линкуем системную библиотеку загрузки DLL
if(NOT WIN32)
    target_link_libraries(3DAndEngineCore INTERFACE ${CMAKE_DL_LIBS})
endif()

# ==============================================================================
# 2. ИСПОЛНЯЕМЫЙ ФАЙЛ (ENGINE RUNTIME)
# ==============================================================================
file(GLOB_RECURSE ENGINE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(3DAndEngine ${ENGINE_SOURCES})
target_link_libraries(3DAndEngine PRIVATE 3DAndEngineCore)

# Создаем структуру папок SDK после сборки
add_custom_command(TARGET 3DAndEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin/plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin/assets"
    COMMENT "[Core] Finalizing SDK structure in bin/ directory"
)

# Сообщение о готовности версии 0.3.0
message(STATUS "-------------------------------------------------------")
message(STATUS " 3DAndEngine Core v${PROJECT_VERSION} configured successfully")
message(STATUS " Target: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS " Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "-------------------------------------------------------")
