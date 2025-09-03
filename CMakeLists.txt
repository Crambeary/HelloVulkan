cmake_minimum_required(VERSION 3.31)
project(HelloVulkan)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (MSVC)
    add_compile_options(-Wall)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# add the executable
add_executable(${PROJECT_NAME} src/main.cpp)

# disable automatic C++20 modules scanning that injects -fmodules-ts flags
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API OFF CACHE BOOL "" FORCE)

# Let cmake find the custom FindVulkan.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)


find_program(SLANGC_EXECUTABLE NAMES slangc slangc.exe
  PATHS $ENV{VULKAN_SDK}/bin
  NO_DEFAULT_PATH
)
if(NOT SLANGC_EXECUTABLE)
    # fall back to path
    find_program(SLANGC_EXECUTABLE NAMES slangc slangc.exe)
endif()
if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "Could not find 'slangc' executable. Install Vulkan SDK or add slangc to PATH.")
endif()


target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(HelloVulkan PRIVATE stb)


function(add_slang_shader_target TARGET)
    if(NOT TARGET)
        message(FATAL_ERROR "add_slang_shader_target: first arg must be the target name")
    endif()

    # remaining args are shader sources
    if(ARGN)
        set(SHADER_SOURCES ${ARGN})
    else()
        message(FATAL_ERROR "Usage: add_slang_shader_target(<target> <shader1.slang> [shader2.slang ...])")
    endif()

    # make output dir in build tree
    set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

    # output filename - use target name so multiple targets don't clobber each other
    set(OUT_SPV "${SHADER_OUT_DIR}/${TARGET}.spv")

    # entry points - change if your entry point names differ
    set(ENTRY_ARGS -entry vertMain -entry fragMain)

    add_custom_command(
    OUTPUT "${OUT_SPV}"
    COMMAND ${SLANGC_EXECUTABLE}
            ${SHADER_SOURCES}
            -target spirv
            -profile spirv_1_4
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            ${ENTRY_ARGS}
            -o "${OUT_SPV}"
    DEPENDS ${SHADER_SOURCES} ${SLANGC_EXECUTABLE}
    WORKING_DIRECTORY "${SHADER_OUT_DIR}"
    COMMENT "Compiling Slang shader(s) -> ${OUT_SPV}"
    VERBATIM
  )

    add_custom_target(${TARGET} DEPENDS "${OUT_SPV}")
endfunction()

add_slang_shader_target( slang_shaders "${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang")
add_dependencies(${PROJECT_NAME} slang_shaders)
set(GENERATED_SHADER_SPD "${CMAKE_BINARY_DIR}/shaders/slang.spv" CACHE FILEPATH "Generated SPIR-V shader")

ADD_CUSTOM_TARGET(distclean
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}"
    COMMENT "Remove the entire build directory"
)
