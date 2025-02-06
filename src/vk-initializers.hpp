#pragma once

#include <vk-types.hpp>

namespace vkinit {
	VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.flags = flags;
		commandPoolInfo.queueFamilyIndex = queueFamilyIndex;

		return commandPoolInfo;
	}

	VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1) {
		VkCommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.commandPool = pool;
		commandBufferInfo.commandBufferCount = count;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		return commandBufferInfo;
	}

	VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0) {
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = flags;

		return fenceInfo;
	}

	VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreInfo.flags = flags;
		return semaphoreInfo;
	}

	VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0) {
		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = flags;

		return commandBufferBeginInfo;
	}

	VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask) {
		VkImageSubresourceRange subImage{};
		subImage.aspectMask = aspectMask;
		subImage.baseMipLevel = 0;
		subImage.levelCount = VK_REMAINING_MIP_LEVELS;
		subImage.baseArrayLayer = 0;
		subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

		return subImage;
	}

	VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
		VkSemaphoreSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.semaphore = semaphore;
		submitInfo.stageMask = stageMask;
		submitInfo.deviceIndex = 0;
		submitInfo.value = 1;

		return submitInfo;
	}

	VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd) {
		VkCommandBufferSubmitInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		info.pNext = nullptr;
		info.commandBuffer = cmd;
		info.deviceMask = 0;

		return info;
	}

	VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo *cmd, VkSemaphoreSubmitInfo *signalSemaphoreInfo, VkSemaphoreSubmitInfo *waitSemaphoreInfo) {
		VkSubmitInfo2 info{};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
		info.pWaitSemaphoreInfos = waitSemaphoreInfo;

		info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
		info.pSignalSemaphoreInfos = signalSemaphoreInfo;

		info.commandBufferInfoCount = 1;
		info.pCommandBufferInfos = cmd;

		return info;
	}
}