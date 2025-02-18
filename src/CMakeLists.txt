add_executable(ComputePlayer
    main.cpp
    vk-engine.cpp
    vk-engine.hpp
    vk-types.hpp
    vk-initializers.hpp
    vk-images.hpp
    vk-descriptors.hpp
    vk-descriptors.cpp
    vk-pipelines.hpp)

set_property(TARGET ComputePlayer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ComputePlayer>")

target_include_directories(ComputePlayer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ComputePlayer vkbootstrap vma glm tinyobjloader imgui stb_image spdlog)
target_link_libraries(ComputePlayer Vulkan::Vulkan sdl2)

add_dependencies(ComputePlayer Shaders)