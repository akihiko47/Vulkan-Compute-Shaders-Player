cmake_minimum_required (VERSION 3.8)

project ("VulkanRenderer")
set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

add_subdirectory(dependencies)
add_subdirectory(src)