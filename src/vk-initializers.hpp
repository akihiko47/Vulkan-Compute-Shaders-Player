#pragma once

#include <vk-types.hpp>

namespace vkinit {
	VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.pNext = nullptr;
		commandPoolInfo.flags = flags;
		commandPoolInfo.queueFamilyIndex = queueFamilyIndex;

		return commandPoolInfo;
	}

	VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1) {
		VkCommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.pNext = nullptr;
		commandBufferInfo.commandPool = pool;
		commandBufferInfo.commandBufferCount = count;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		return commandBufferInfo;
	}
	
}