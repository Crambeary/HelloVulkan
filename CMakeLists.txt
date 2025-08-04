cmake_minimum_required(VERSION 3.31)
project(HelloVulkan)

# Let cmake find the custom FindVulkan.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)


# add the executable
add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw)


# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

