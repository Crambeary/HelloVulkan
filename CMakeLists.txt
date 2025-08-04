cmake_minimum_required(VERSION 3.31)
project(HelloVulkan)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

string(REPLACE "-fmodules-ts" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Let cmake find the custom FindVulkan.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)


# add the executable
add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw)



