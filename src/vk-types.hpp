#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <iostream>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#define FMT_UNICODE 0
#include <spdlog/spdlog.h>

#define VK_CHECK(x)                                                                   \
    do {                                                                              \
        VkResult err = x;                                                             \
        if (err) {                                                                    \
            spdlog::critical("Detected Vulkan error: {}", string_VkResult(err));      \
            abort();                                                                  \
        }                                                                             \
    } while (0)