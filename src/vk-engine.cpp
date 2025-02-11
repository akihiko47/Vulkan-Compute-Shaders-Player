#include "vk-engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <vk-initializers.hpp>
#include <vk-pipelines.hpp>
#include <vk-types.hpp>
#include <vk-images.hpp>

#include <chrono>
#include <thread>
#include <filesystem>


const bool USE_VALIDATION_LAYERS = true;


using namespace vr;

VulkanEngine::VulkanEngine() : m_isInitialized(false), m_frameNumber(0), m_stopRendering(false), m_windowExtent{800, 800}, m_currentComputeEffect(0) {
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
	InitPipelines();
	InitImgui();

	m_isInitialized = true;

	spdlog::info("Renderer initialized");
}


void VulkanEngine::Cleanup() {
	if (m_isInitialized) {
		vkDeviceWaitIdle(m_device);

		m_mainDeletionQueue.flush();
		for (auto &frame : m_frames) {
			vkDestroyCommandPool(m_device, frame.commandPool, nullptr);

			vkDestroyFence(m_device, frame.renderFence, nullptr);
			vkDestroySemaphore(m_device, frame.renderSemaphore, nullptr);
			vkDestroySemaphore(m_device, frame.swapchainSemaphore, nullptr);

			frame.deletionQueue.flush();
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

	SDL_WindowFlags windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
		spdlog::critical("SDL_GetDesktopDisplayMode failed: {}", SDL_GetError());
		exit(1);
	}
	spdlog::info("Rendering to image with resolution: width={}px, height={}px", dm.w, dm.h);

	VkExtent3D renderImageExtent{};
	renderImageExtent.width = dm.w;
	renderImageExtent.height = dm.h;
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
}


void VulkanEngine::DestroySwapChain() {
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

	// deletes images as well
	for (auto &swapChainImageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device, swapChainImageView, nullptr);
	}
}


void VulkanEngine::RecreateSwapChain() {
	vkDeviceWaitIdle(m_device);

	DestroySwapChain();

	int w, h;
	SDL_GetWindowSize(m_window, &w, &h);
	m_windowExtent.width = w;
	m_windowExtent.height = h;

	CreateSwapChain(m_windowExtent.width, m_windowExtent.height);

	spdlog::info("Swap chain recreation");
	m_resizeRequested = false;
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

	// create command pool and buffer for immediate commands
	VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_immCommandPool));

	VkCommandBufferAllocateInfo commandBufferInfo = vkinit::CommandBufferAllocateInfo(m_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferInfo, &m_immCommandBuffer));

	m_mainDeletionQueue.PushFunction([&]() {
		vkDestroyCommandPool(m_device, m_immCommandPool, nullptr);
	});
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

	// create fence for immediate commands
	VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_immFence));
	m_mainDeletionQueue.PushFunction( [&]() { vkDestroyFence(m_device, m_immFence, nullptr); });
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


void VulkanEngine::InitPipelines() {
	std::string shadersPath;
	#ifdef SHADERS_DIR
		const char* shadersDir = SHADERS_DIR;
		shadersPath = std::string(shadersDir) + "/";
	#endif

	for (auto &p : std::filesystem::recursive_directory_iterator(shadersPath)) {
		if (!(p.path().extension() == ".spv")) {
			continue;  // look only for compiled shaders
		}

		// initialize effect struct
		ComputeEffect effect{};
		effect.name = p.path().stem().stem().string();

		// create pipeline layout
		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.pSetLayouts = &m_renderImageDescriptorLayout;
		layoutInfo.setLayoutCount = 1;

		VkPushConstantRange pushConstants{};
		pushConstants.offset = 0;
		pushConstants.size = sizeof(ComputePushConstants);
		pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		layoutInfo.pPushConstantRanges = &pushConstants;
		layoutInfo.pushConstantRangeCount = 1;

		VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &effect.layout));

		// create shader module
		std::string shaderPath = p.path().string();

		if (!std::filesystem::exists(shaderPath)) {
			spdlog::error("no such shader: {}\n", shaderPath);
		}

		VkShaderModule shaderModule;
		if (!vkutils::LoadShaderModule(shaderPath.c_str(), m_device, &shaderModule)) {
			spdlog::error("error when building the compute shader\n");
		}

		// create pipeline
		VkPipelineShaderStageCreateInfo stageInfo{};
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stageInfo.module = shaderModule;
		stageInfo.pName = "main";

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = effect.layout;
		computePipelineCreateInfo.stage = stageInfo;

		VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &effect.pipeline));

		// add effect to effects list
		m_computeEffects.push_back(effect);

		// destroy shader module and sent pipeline to deletion queue
		vkDestroyShaderModule(m_device, shaderModule, nullptr);
		m_mainDeletionQueue.PushFunction([=]() {
			vkDestroyPipelineLayout(m_device, effect.layout, nullptr);
			vkDestroyPipeline(m_device, effect.pipeline, nullptr);
		});
	}

}


void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function) {
	VK_CHECK(vkResetFences(m_device, 1, &m_immFence));
	VK_CHECK(vkResetCommandBuffer(m_immCommandBuffer, 0));

	VkCommandBuffer cmd = m_immCommandBuffer;
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo submitInfo = vkinit::CommandBufferSubmitInfo(cmd);
	VkSubmitInfo2 submit = vkinit::SubmitInfo(&submitInfo, nullptr, nullptr);

	// submit command buffer and block fence
	VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, m_immFence));
	VK_CHECK(vkWaitForFences(m_device, 1, &m_immFence, true, 9999999999));
}


void VulkanEngine::InitImgui() {
	std::vector<VkDescriptorPoolSize> poolSizes{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(m_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_instance;
	init_info.PhysicalDevice = m_physicalDevice;
	init_info.Device = m_device;
	init_info.Queue = m_graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	VkPipelineRenderingCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

	init_info.PipelineRenderingCreateInfo = pipelineInfo;
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChainImageFormat;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	m_mainDeletionQueue.PushFunction([=]() {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
	});
}


void VulkanEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView) {
	VkRenderingAttachmentInfo colorAttachment = vkinit::AttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::RenderingInfo(m_swapChainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
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

			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.scancode == SDL_SCANCODE_F11) {
					if (!m_isFullscreen) {
						SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
					} else {
						SDL_SetWindowFullscreen(m_window, 0);
					}
					m_isFullscreen = !m_isFullscreen;
				}
			}

			// send SDL event to imgui for handling
			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		if (m_stopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

		if (m_resizeRequested) {
			RecreateSwapChain();
		}

		UpdateTime();
		AddImguiWindows();

        Draw();
	}
}


void VulkanEngine::AddImguiWindows() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::Begin("Shaders selector")) {
		ComputeEffect& effect = m_computeEffects[m_currentComputeEffect];

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

		ImGui::Text(effect.name.c_str());

		ImGui::SliderInt("Effect Index", &m_currentComputeEffect, 0, m_computeEffects.size() - 1);

		ImGui::ColorEdit4("data 2", (float*)&effect.data.data2);
		ImGui::ColorEdit4("data 3", (float*)&effect.data.data3);
		ImGui::ColorEdit4("data 4", (float*)&effect.data.data4);
	}
	ImGui::End();

	ImGui::Render();
}


void VulkanEngine::UpdateTime() {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	m_currentFrameTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	m_deltaTime = m_currentFrameTime - m_lastFrameTime;
	m_totalTime += m_deltaTime;
	m_lastFrameTime = m_currentFrameTime;
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
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		m_resizeRequested = true;
	}

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



	// use compute shader pipeline
	ComputeEffect &effect = m_computeEffects[m_currentComputeEffect];
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &m_renderImageDescriptors, 0, nullptr);

	// push constants
	effect.data.data1.x = m_totalTime;
	effect.data.data1.y = static_cast<float>(m_swapChainExtent.width) / m_swapChainExtent.height;
	vkCmdPushConstants(commandBuffer, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	// execute command pipeline
	vkCmdDispatch(commandBuffer, std::ceil(m_renderExtent.width / 16.0), std::ceil(m_renderExtent.height / 16.0), 1);



	// transition render image and swapchain image to correct layouts for transfer
	vkutils::TransitionImageLayout(commandBuffer, m_renderImage.image,           VK_IMAGE_LAYOUT_GENERAL,   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutils::TransitionImageLayout(commandBuffer, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy render image to swapchain image
	vkutils::CopyImageToImage(commandBuffer, m_renderImage.image, m_swapChainImages[imageIndex], m_renderExtent, m_swapChainExtent);



	// transition swapchain image layout to render to it
	vkutils::TransitionImageLayout(commandBuffer, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// draw imgui into swapchain image
	DrawImgui(commandBuffer, m_swapChainImageViews[imageIndex]);

	// transition swap chain image to presentable layout
	vkutils::TransitionImageLayout(commandBuffer, m_swapChainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);



	// end recording
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// submit command buffer to queue
	VkCommandBufferSubmitInfo cmdInfo = vkinit::CommandBufferSubmitInfo(commandBuffer);

	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

	VkSubmitInfo2 submit = vkinit::SubmitInfo(&cmdInfo, &signalInfo, &waitInfo);
	VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

	// present rendered image
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &imageIndex;

	VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
		m_resizeRequested = true;
	}

	// increase number of frames
	m_frameNumber++;
}