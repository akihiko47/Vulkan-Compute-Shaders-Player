add_executable(VulkanRenderer
    main.cpp
    vk-engine.cpp
    vk-engine.hpp
    vk-types.hpp
    vk-initializers.hpp
    vk-images.hpp)

set_property(TARGET VulkanRenderer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VulkanRenderer>")

target_include_directories(VulkanRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(VulkanRenderer vkbootstrap vma glm tinyobjloader imgui stb_image spdlog)
target_link_libraries(VulkanRenderer Vulkan::Vulkan sdl2)