add_executable(VulkanRenderer
    main.cpp)

target_include_directories(VulkanRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(VulkanRenderer vkbootstrap vma glm tinyobjloader imgui stb_image)
target_link_libraries(VulkanRenderer Vulkan::Vulkan sdl2)