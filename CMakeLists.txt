cmake_minimum_required(VERSION 3.15)
project(3DAndEngine LANGUAGES CXX)

# ==============================================================================
# 1. ГЛОБАЛЬНЫЕ ПУТИ ВЫВОДА СБОРКИ
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Поиск внешних зависимостей (vcpkg)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 3. Исходники статического ядра движка
set(CORE_LIBRARY_SRC
    src/core/Application.cpp
    src/core/ConfigService.cpp
    src/core/EventBus.cpp
    src/core/LibraryLoader.cpp
    src/core/ServiceManager.cpp
)

# 4. Сборка статической библиотеки ядра
add_library(engine_core_lib STATIC ${CORE_LIBRARY_SRC})

# Подключаем общую папку инклюдов include/core/
target_include_directories(engine_core_lib PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(engine_core_lib PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json fmt::fmt)

# 5. Главный бинарник движка (EXE)
add_executable(3DAndEngine src/main.cpp)

# Даем исполняемому файлу доступ ко всем инклюдам ядра, чтобы main.cpp 
# мог безопасно инициализировать и запускать core::Application
target_include_directories(3DAndEngine PRIVATE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(3DAndEngine PRIVATE engine_core_lib)

# 6. Тонкая настройка компилятора под стандарты MSVC
if(MSVC)
    target_compile_definitions(3DAndEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(engine_core_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    
    # Жестко привязываем Multi-Threaded DLL рантайм, чтобы убрать 
    # любые скрытые конфликты стандартных строк std::string между плагинами
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
