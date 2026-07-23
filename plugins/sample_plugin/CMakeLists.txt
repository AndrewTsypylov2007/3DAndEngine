cmake_minimum_required(VERSION 3.10)

add_library(sample_plugin SHARED sample_plugin.cpp)
target_include_directories(sample_plugin PRIVATE ${CMAKE_SOURCE_DIR}/include)

# На Windows по умолчанию создаётся .dll, на *nix .so, это нормально.

# Копируем собранный плагин в папку с исполняемым файлом engine_core после сборки
add_custom_command(TARGET sample_plugin POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sample_plugin> $<TARGET_FILE_DIR:engine_core>
	COMMENT "Copy sample_plugin to engine_core output directory"
)
