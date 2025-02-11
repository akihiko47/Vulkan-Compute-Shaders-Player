#pragma once

#include <vk-types.hpp>
#include <vk-descriptors.hpp>


const uint32_t FRAMES_IN_FLIGHT = 2;

struct SDL_Window;

namespace vr {
	class VulkanEngine final {
	public:
		VulkanEngine();
		~VulkanEngine();

		void Run();

	private:
		void Init();
		void Draw();
		void Cleanup();

		// sdl window creation
		void CreateSDLWindow();

		// vulkan initialization
		void InitVulkan();
		void InitSwapchain();
		void CreateSwapChain(uint32_t width, uint32_t height);
		void DestroySwapChain();
		void RecreateSwapChain();
		void InitCommands();
		void InitSyncStructures();

		// frames in flight
		FrameData &GetCurrentFrame() { return m_frames[m_frameNumber % FRAMES_IN_FLIGHT]; }

		// descriptors
		void InitDescriptors();

		// pipelines
		void InitPipelines();

		// immediate command that are submitted outside of main render loop
		void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);

		// imgui
		void InitImgui();
		void AddImguiWindows();
		void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

		// time
		void UpdateTime();

	private:
		bool            m_isInitialized;
		uint32_t        m_frameNumber;
		bool            m_stopRendering;
		VkExtent2D      m_windowExtent;

		// time
		float m_totalTime = 0;
		float m_lastFrameTime = 0;
		float m_currentFrameTime = 0;
		float m_deltaTime = 0;

		// sdl window stuff
		SDL_Window *m_window;
		bool        m_isFullscreen = false;

		// vulkan stuff
		VkInstance               m_instance;
		VkDebugUtilsMessengerEXT m_debugMessenger;
		VkPhysicalDevice         m_physicalDevice;
		VkDevice                 m_device;
		VkSurfaceKHR             m_surface;

		// swapchain stuff
		VkSwapchainKHR           m_swapChain;
		VkFormat                 m_swapChainImageFormat;
		std::vector<VkImage>     m_swapChainImages;
		std::vector<VkImageView> m_swapChainImageViews;
		VkExtent2D               m_swapChainExtent;
		bool                     m_resizeRequested = false;

		// queues stuff
		VkQueue  m_graphicsQueue;
		uint32_t m_graphicsQueueFamily;

		// frames in flight stuff
		FrameData m_frames[FRAMES_IN_FLIGHT];

		// deletion
		DeletionQueue m_mainDeletionQueue;

		// allocation
		VmaAllocator m_allocator;

		// render image
		AllocatedImage m_renderImage;
		VkExtent2D     m_renderExtent;

		// descriptors
		DescriptorAllocator   m_globalDescriptorAllocator;
		VkDescriptorSet       m_renderImageDescriptors;
		VkDescriptorSetLayout m_renderImageDescriptorLayout;

		// pipelines (this struct holds pipeline layout and pipeline)
		std::vector<ComputeEffect> m_computeEffects;
		int m_currentComputeEffect;

		// immediate command that are submitted outside of main render loop
		VkFence         m_immFence;
		VkCommandBuffer m_immCommandBuffer;
		VkCommandPool   m_immCommandPool;
	};
}