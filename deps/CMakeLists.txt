cmake_minimum_required(VERSION 3.10)

project(deps)

add_subdirectory(stb_image)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(spdlog)
add_subdirectory(volk)
add_subdirectory(imgui)

set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)
add_subdirectory(glslang)
add_subdirectory(SPIRV-Headers)

set(SHADERC_SKIP_TESTS ON)
set(SHADERC_ENABLE_TESTS OFF)
add_subdirectory(shaderc)

add_subdirectory(tinyxml2)

set(MSDF_ATLAS_BUILD_STANDALONE OFF)
set(MSDF_ATLAS_USE_VCPKG OFF)
set(MSDF_ATLAS_USE_SKIA OFF)
set(MSDF_ATLAS_NO_ARTERY_FONT OFF)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL OFF)
set(MSDF_ATLAS_INSTALL OFF)
set(MSDF_ATLAS_DYNAMIC_RUNTIME OFF)
# set(BUILD_SHARED_LIBS OFF)

set(MSDFGEN_CORE_ONLY ON)
set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_USE_VCPKG OFF)
set(MSDFGEN_USE_OPENMP OFF)
set(MSDFGEN_USE_CPP11 OFF)
set(MSDFGEN_USE_SKIA OFF)
set(MSDFGEN_INSTALL OFF)
set(MSDFGEN_DYNAMIC_RUNTIME OFF)
# set(BUILD_SHARED_LIBS OFF)

add_subdirectory(msdf-atlas-gen)