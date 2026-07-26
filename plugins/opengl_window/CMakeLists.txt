cmake_minimum_required(VERSION 3.14)

add_library(opengl_window SHARED opengl_window.cpp)

target_include_directories(opengl_window PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Prefer finding installed 3DAndEngine package; fall back to including core source
find_package(3DAndEngine CONFIG)
if (3DAndEngine_FOUND)
  target_link_libraries(opengl_window PRIVATE 3DAndEngine::engine_core_lib)
  target_include_directories(opengl_window PRIVATE ${3DAndEngine_INCLUDE_DIRS})
else()
  message(STATUS "3DAndEngine not found via find_package; using source include directory")
  target_include_directories(opengl_window PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

# Link with glfw and glad (provided by vcpkg in CI/local via find_package)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
target_link_libraries(opengl_window PRIVATE glfw glad)

# Copy plugin to engine_core output directory after build
add_custom_command(TARGET opengl_window POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:opengl_window> $<TARGET_FILE_DIR:engine_core>
	COMMENT "Copy opengl_window to engine_core output directory"
)
