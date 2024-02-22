cmake_minimum_required(VERSION 3.10)
include(CMakePrintHelpers)

project(imgui)

file(GLOB SRC_FILE imgui/*.cpp)
file(GLOB MISC_SRC_FILE imgui/misc/cpp/*.cpp)

set(BACKEND_DIR imgui/backends)

set(BACKEND_SRC ${BACKEND_DIR}/imgui_impl_glfw.cpp ${BACKEND_DIR}/imgui_impl_vulkan.cpp)

add_library(imgui
    ${SRC_FILE}
    ${BACKEND_SRC}
    ${MISC_SRC_FILE}
)

# cmake_print_variables(MISC_SRC_FILE)

add_compile_definitions(imgui
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
)

include_directories(
    PUBLIC ../glfw/include
)

target_include_directories(imgui
    PUBLIC imgui
)