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

namespace vr {
	// structure to delete vulkan objects in correct order
	struct DeletionQueue {
		std::deque<std::function<void()>> deletors;

		void PushFunction(std::function<void()> &&function) {
			deletors.push_back(function);
		}

		void flush() {
			for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
				(*it)();
			}

			deletors.clear();
		}
	};

	// structures and commands needed to draw one frame in flight
	struct FrameData {
		VkCommandPool   commandPool;
		VkCommandBuffer mainCommandBuffer;
		VkSemaphore     swapchainSemaphore;
		VkSemaphore     renderSemaphore;
		VkFence         renderFence;
		DeletionQueue   deletionQueue;
	};

	struct AllocatedImage {
		VkImage image;
		VkImageView imageView;
		VmaAllocation allocation;
		VkExtent3D imageExtent;
		VkFormat imageFormat;
	};

	struct ComputePushConstants {
		glm::vec4 data1;
		glm::vec4 data2;
		glm::vec4 data3;
		glm::vec4 data4;
	};

	struct ComputeEffect {
		const char *name;
		VkPipeline pipeline;
		VkPipelineLayout layout;
		ComputePushConstants data;
	};
}

