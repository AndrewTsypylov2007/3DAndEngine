cmake_minimum_required(VERSION 3.15)
project(3DAndEngine LANGUAGES CXX)

# Опция для сборки модульных тестов
option(BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Находим внешние зависимости через vcpkg / системные пакеты
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 2. Явный список исходных файлов ядра движка (без main.cpp)
set(CORE_LIBRARY_SRC
    src/core/Application.cpp
    src/core/ConfigService.cpp
    src/core/EventBus.cpp
    src/core/LoggerService.cpp
    src/core/LibraryLoader.cpp
    src/core/ServiceManager.cpp
    src/core/TaskScheduler.cpp
)

# 3. Собираем ядро как динамическую библиотеку (SHARED DLL)
add_library(engine_core_lib SHARED ${CORE_LIBRARY_SRC})

set_target_properties(engine_core_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

target_include_directories(engine_core_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Привязываем внешние библиотеки к движку
target_link_libraries(engine_core_lib PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json fmt::fmt)

# 4. Собираем исполняемый файл приложения (главный бинарник движка)
add_executable(3DAndEngine src/main.cpp)
target_link_libraries(3DAndEngine PRIVATE engine_core_lib)

if(MSVC)
    target_compile_definitions(3DAndEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(engine_core_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 5. Сборка модульных тестов
if(BUILD_TESTS)
    enable_testing()
    
    # Исходники самого тестового движка
    set(TEST_SRC
        tests/unit_tests.cpp
        tests/test_servicemanager.cpp
    )
    
    add_executable(unit_tests ${TEST_SRC})
    
    # Тесты просто линкуются с готовой библиотекой ядра движка
    target_link_libraries(unit_tests PRIVATE engine_core_lib)
    target_include_directories(unit_tests PRIVATE tests)
    target_compile_definitions(unit_tests PRIVATE UNIT_TESTS_MAIN=1)
    
    add_test(NAME core_unit_tests COMMAND unit_tests)
endif()

# ==============================================================================
# 6. Правила генерации SDK (Экспорт для внешних разработчиков)
# ==============================================================================
include(CMakePackageConfigHelpers)

# ИСПРАВЛЕННАЯ СТРОГАЯ ИНСТАЛЛЯЦИЯ:
# Явно разделяем типы объектов, чтобы CMake на Windows не терял исполняемый файл
install(TARGETS engine_core_lib 3DAndEngine
    EXPORT 3DAndEngineTargets
    RUNTIME DESTINATION bin         # Сюда улетит engine_core_lib.dll
    EXECUTABLE DESTINATION bin      # ЖЕЛЕЗОБЕТОННО: Сюда улетит 3DAndEngine.exe
    LIBRARY DESTINATION lib         # Для Linux (.so) и macOS (.dylib)
    ARCHIVE DESTINATION lib         # Сюда улетит файл импорта engine_core_lib.lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)


install(EXPORT 3DAndEngineTargets
    FILE 3DAndEngineTargets.cmake
    NAMESPACE 3DAndEngine::
    DESTINATION lib/cmake/3DAndEngine
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/3DAndEngineConfigVersion.cmake
    VERSION 0.1.0
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/3DAndEngineConfigVersion.cmake
    DESTINATION lib/cmake/3DAndEngine
)
