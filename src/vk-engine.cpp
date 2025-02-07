#include "vk-engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vk-initializers.hpp>
#include <vk-types.hpp>
#include <vk-images.hpp>

#include <chrono>
#include <thread>


const bool USE_VALIDATION_LAYERS = true;


using namespace vr;

VulkanEngine::VulkanEngine() : m_isInitialized(false), m_frameNumber(0), m_stopRendering(false), m_windowExtent{800, 800} {
	Init();
}


VulkanEngine::~VulkanEngine() {
	Cleanup();
}


void VulkanEngine::Init() {
	spdlog::info("Renderer initialization started");

	CreateSDLWindow();
	InitVulkan();
	InitSwapchain();
	InitCommands();
	InitSyncStructures();
	InitDescriptors();

	m_isInitialized = true;

	spdlog::info("Renderer initialized");
}


void VulkanEngine::Cleanup() {
	if (m_isInitialized) {
		vkDeviceWaitIdle(m_device);

		for (auto &frame : m_frames) {
			vkDestroyCommandPool(m_device, frame.commandPool, nullptr);

			vkDestroyFence(m_device, frame.renderFence, nullptr);
			vkDestroySemaphore(m_device, frame.renderSemaphore, nullptr);
			vkDestroySemaphore(m_device, frame.swapchainSemaphore, nullptr);

			frame.deletionQueue.flush();
		}

		m_mainDeletionQueue.flush();
	}

	spdlog::info("Renderer cleaned up");
}


void VulkanEngine::CreateSDLWindow() {

	// we want to use window and input systems from SDL
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN);

	m_window = SDL_CreateWindow(
		"Vulkan Renderer",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_windowExtent.width,
		m_windowExtent.height,
		windowFlags
	);
	m_mainDeletionQueue.PushFunction( [&]() {SDL_DestroyWindow(m_window); } );
}


void VulkanEngine::InitVulkan() {

	// instance creation
	vkb::InstanceBuilder instanceBuilder;

	vkb::Result instanceRes = instanceBuilder.set_app_name("VulkanRenderer")
		.request_validation_layers(USE_VALIDATION_LAYERS)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();
	vkb::Instance vkbInstance= instanceRes.value();

	m_instance = vkbInstance.instance;
	m_debugMessenger = vkbInstance.debug_messenger;

	m_mainDeletionQueue.PushFunction( [&]() { vkDestroyInstance(m_instance, nullptr); } );
	m_mainDeletionQueue.PushFunction( [&]() { vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger); } );

	// surface creation
	SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);
	m_mainDeletionQueue.PushFunction( [&]() { vkDestroySurfaceKHR(m_instance, m_surface, nullptr); } );

	// physical device creation
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector selector{vkbInstance};
	vkb::PhysicalDevice vkbPhysicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features13)
		.set_required_features_12(features12)
		.set_surface(m_surface)
		.select()
		.value();

	m_physicalDevice = vkbPhysicalDevice.physical_device;

	// device creation
	vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
	vkb::Device vkbDevice = deviceBuilder.build().value();

	m_device = vkbDevice.device;

	m_mainDeletionQueue.PushFunction( [&]() { vkDestroyDevice(m_device, nullptr); } );

	// queues creation
	m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	// allocator creation
	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_physicalDevice;
	allocatorInfo.device = m_device;
	allocatorInfo.instance = m_instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &m_allocator);
	m_mainDeletionQueue.PushFunction( [&]() { vmaDestroyAllocator(m_allocator); } );
}


void VulkanEngine::InitSwapchain() {
	CreateSwapChain(m_windowExtent.width, m_windowExtent.height);

	// render image allocation
	VkExtent3D renderImageExtent{};
	renderImageExtent.width = m_windowExtent.width;
	renderImageExtent.height = m_windowExtent.height;
	renderImageExtent.depth = 1;

	m_renderImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_renderImage.imageExtent = renderImageExtent;

	VkImageUsageFlags renderImageUsages{};
	renderImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	renderImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	renderImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;          // for compute shader
	renderImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // to use in graphics pipeline

	VkImageCreateInfo imgInfo = vkinit::ImageCreateInfo(m_renderImage.imageFormat, renderImageUsages, renderImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo imgAllocInfo{};
	imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create image
	vmaCreateImage(m_allocator, &imgInfo, &imgAllocInfo, &m_renderImage.image, &m_renderImage.allocation, nullptr);

	// create image view
	VkImageViewCreateInfo imgViewInfo = vkinit::ImageviewCreateInfo(m_renderImage.imageFormat, m_renderImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(m_device, &imgViewInfo, nullptr, &m_renderImage.imageView));

	// add to deletion queue
	m_mainDeletionQueue.PushFunction([&]() {
		vkDestroyImageView(m_device, m_renderImage.imageView, nullptr);
		vmaDestroyImage(m_allocator, m_renderImage.image, m_renderImage.allocation);
	});
}


void VulkanEngine::CreateSwapChain(uint32_t width, uint32_t height) {
	vkb::SwapchainBuilder swapChainBuilder{m_physicalDevice, m_device, m_surface};

	m_swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	VkSurfaceFormatKHR surfaceFormat{};
	surfaceFormat.format = m_swapChainImageFormat;
	surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	vkb::Swapchain vkbSwapchain = swapChainBuilder
		.set_desired_format(surfaceFormat)
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	m_swapChainExtent = vkbSwapchain.extent;
	m_swapChain = vkbSwapchain.swapchain;
	m_swapChainImages = vkbSwapchain.get_images().value();
	m_swapChainImageViews = vkbSwapchain.get_image_views().value();

	m_mainDeletionQueue.PushFunction( [&]() { DestroySwapChain(); } );
}


void VulkanEngine::DestroySwapChain() {
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

	// deletes images as well
	for (auto &swapChainImageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device, swapChainImageView, nullptr);
	}
}


void VulkanEngine::InitCommands() {
	// create command pools for each frame in flight
	// this flag allows us to reset individual command buffer
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::CommandPoolCreateInfo(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i != FRAMES_IN_FLIGHT; ++i) {
		VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_frames[i].commandPool));

		// allocate command buffer using this command pool
		VkCommandBufferAllocateInfo commandBufferInfo = vkinit::CommandBufferAllocateInfo(m_frames[i].commandPool);

		VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferInfo, &m_frames[i].mainCommandBuffer));
	}
}


void VulkanEngine::InitSyncStructures() {
	// create fence to controll cpu between frames
	// fence must be in signalled state at start
	// and 2 semaphores to sync gpu in one frame
	
	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();

	for (auto &frame : m_frames) {
		VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &frame.renderFence));
		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &frame.swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore));
	}
}


void VulkanEngine::InitDescriptors() {
	// create pool
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
	};

	m_globalDescriptorAllocator.InitPool(m_device, 10, sizes);

	// get layout that matches pool
	{
		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		m_renderImageDescriptorLayout = builder.Build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	// allocate descriptor sets
	m_renderImageDescriptors = m_globalDescriptorAllocator.Allocate(m_device, m_renderImageDescriptorLayout);

	// update descriptor sets
	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.imageView = m_renderImage.imageView;

	VkWriteDescriptorSet renderImageWrite{};
	renderImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	renderImageWrite.dstBinding = 0;
	renderImageWrite.dstSet = m_renderImageDescriptors;
	renderImageWrite.descriptorCount = 1;
	renderImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	renderImageWrite.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(m_device, 1, &renderImageWrite, 0, nullptr);

	// add to destruction queue
	m_mainDeletionQueue.PushFunction([&]() {
		m_globalDescriptorAllocator.DestroyPool(m_device);
		vkDestroyDescriptorSetLayout(m_device, m_renderImageDescriptorLayout, nullptr);
	});
}


void VulkanEngine::Run() {
	SDL_Event e;
	bool bQuit = false;

	while (!bQuit) {
		while (SDL_PollEvent(&e) != 0) {
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_QUIT)
				bQuit = true;

			if (e.type == SDL_WINDOWEVENT) {
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					m_stopRendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					m_stopRendering = false;
				}
			}
		}

		if (m_stopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Draw();
	}
}


void VulkanEngine::Draw() {
	// wait untill gpu has finished rendering the last frame
	// we can wait no more than 1 second
	VK_CHECK(vkWaitForFences(m_device, 1, &GetCurrentFrame().renderFence, true, 1000000000));

	// delete per frame objects from previous frame
	GetCurrentFrame().deletionQueue.flush();

	// reset fence so that we can wait for it in next frame
	VK_CHECK(vkResetFences(m_device, 1, &GetCurrentFrame().renderFence));

	// get image index from swapchain
	uint32_t imageIndex;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapChain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &imageIndex));

	// reset command buffer (copy because it is just pointer)
	VkCommandBuffer commandBuffer = GetCurrentFrame().mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

	// begin recording
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

	// configure render image extent
	m_renderExtent.width = m_renderImage.imageExtent.width;
	m_renderExtent.height = m_renderImage.imageExtent.height;

	// transition render image to writable layout
	// we dont care about previous layout
	vkutils::TransitionImageLayout(commandBuffer, m_renderImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// clear render image
	VkClearColorValue clearValue;
	float flash = std::abs(std::sin(m_frameNumber / 120.f));
	clearValue = {{ 0.0f, 0.0f, flash, 1.0f }};

	VkImageSubresourceRange clearRange = vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	vkCmdClearColorImage(commandBuffer, m_renderImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	// transition render image and swapchain image to correct layouts for transfer
	vkutils::TransitionImageLayout(commandBuffer, m_renderImage.image,           VK_IMAGE_LAYOUT_GENERAL,   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutils::TransitionImageLayout(commandBuffer, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy render image to swapchain image
	vkutils::CopyImageToImage(commandBuffer, m_renderImage.image, m_swapChainImages[imageIndex], m_renderExtent, m_swapChainExtent);

	// transition swap chain image to presentable layout
	vkutils::TransitionImageLayout(commandBuffer, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// end recording
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// submit command buffer to queue
	VkCommandBufferSubmitInfo cmdInfo = vkinit::CommandBufferSubmitInfo(commandBuffer);

	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

	VkSubmitInfo2 submit = vkinit::SubmitInfo(&cmdInfo, &signalInfo, &waitInfo);
	VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

	// resent rendered image
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &imageIndex;

	VK_CHECK(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

	// increase number of frames
	m_frameNumber++;
}