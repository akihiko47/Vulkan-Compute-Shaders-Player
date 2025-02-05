#include "vk-engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#include <vk-initializers.hpp>
#include <vk-types.hpp>

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

	m_isInitialized = true;

	spdlog::info("Renderer initialized");
}


void VulkanEngine::Cleanup() {
	if (m_isInitialized) {
		vkDeviceWaitIdle(m_device);

		for (auto frame : m_frames) {
			vkDestroyCommandPool(m_device, frame.commandPool, nullptr);
		}

		DestroySwapChain();

		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyDevice(m_device, nullptr);

		vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
		vkDestroyInstance(m_instance, nullptr);
		SDL_DestroyWindow(m_window);
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

	// surface creation
	SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);

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

	// queues creation
	m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}


void VulkanEngine::InitSwapchain() {
	CreateSwapChain(m_windowExtent.width, m_windowExtent.height);
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
}


void VulkanEngine::DestroySwapChain() {
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

	// deletes images as well
	for (auto swapChainImageView : m_swapChainImageViews) {
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

}