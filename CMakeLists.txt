cmake_minimum_required (VERSION 3.8)

project ("VulkanRenderer")
set(CMAKE_CXX_STANDARD 17)


# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message("Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()


find_package(Vulkan REQUIRED)

add_subdirectory(dependencies)
add_subdirectory(src)


if(MSVC)
 target_compile_options(VulkanRenderer PRIVATE "/MP")
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT VulkanRenderer)