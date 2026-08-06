cmake_minimum_required(VERSION 3.20)
project(3DAndEngineCore VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Глобальные флаги для AAA-разработки
if(MSVC)
    # /wd4189 - подавляет "инициализированная, но не используемая переменная" в тестах
    add_compile_options(/W4 /WX /permissive- /wd4201 /wd4189)
else()
    # -Wno-unused-variable - чтобы тесты в Release не ломали сборку на Ubuntu/Mac
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-variable)
endif()

# Главное заголовочное ядро движка
add_library(3DAndEngineCore INTERFACE)
target_include_directories(3DAndEngineCore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NOT WIN32)
    target_link_libraries(3DAndEngineCore INTERFACE ${CMAKE_DL_LIBS})
endif()

# Автоматический сбор исходников
file(GLOB_RECURSE ENGINE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Создание исполняемого файла
add_executable(3DAndEngine ${ENGINE_SOURCES})
target_link_libraries(3DAndEngine PRIVATE 3DAndEngineCore)
