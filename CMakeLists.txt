cmake_minimum_required(VERSION 3.21) # Подняли версию до 3.21 для корректной работы $<TARGET_RUNTIME_DLLS>
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

# Автоматически генерируем заголовочный файл экспорта для Windows (dllexport/dllimport)
include(GenerateExportHeader)
generate_export_header(engine_core_lib
    BASE_NAME ENGINE_CORE
    EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/core/engine_core_export.h
)

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

# Автоматическое копирование сторонних DLL (vcpkg и нашей dll) в папку с .exe при локальной сборке на Windows
if(WIN32)
    add_custom_command(TARGET 3DAndEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:3DAndEngine>
        $<TARGET_FILE_DIR:3DAndEngine>
        COMMAND_EXPAND_LISTS
    )
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
    
    # Копируем DLL к тестам, чтобы ctest на Windows не падал из-за отсутствия зависимостей
    if(WIN32)
        add_custom_command(TARGET unit_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:unit_tests>
            $<TARGET_FILE_DIR:unit_tests>
            COMMAND_EXPAND_LISTS
        )
    endif()
    
    add_test(NAME core_unit_tests COMMAND unit_tests)
endif()

# ==============================================================================
# 6. Правила генерации SDK (Экспорт для внешних разработчиков)
# ==============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS engine_core_lib 3DAndEngine
    EXPORT 3DAndEngineTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
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
