cmake_minimum_required(VERSION 3.20)
project(3DAndEngineCore VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Глобальные флаги для AAA-разработки (максимальные предупреждения компилятора)
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /wd4201)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Главное заголовочное ядро движка
add_library(3DAndEngineCore INTERFACE)
target_include_directories(3DAndEngineCore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NOT WIN32)
    target_link_libraries(3DAndEngineCore INTERFACE ${CMAKE_DL_LIBS})
endif()

# ==============================================================================
# АВТОМАТИЧЕСКИЙ СБОР ИСХОДНИКОВ (Решает проблему регистра букв и путей в CI)
# ==============================================================================
file(GLOB_RECURSE ENGINE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Собираем красивый исполняемый файл на основе всех найденных исходников
add_executable(3DAndEngine ${ENGINE_SOURCES})
target_link_libraries(3DAndEngine PRIVATE 3DAndEngineCore)
