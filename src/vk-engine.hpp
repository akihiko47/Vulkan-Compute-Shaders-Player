#pragma once

#include <vk-types.hpp>


struct SDL_Window;

namespace vr {
	class VulkanEngine {
	public:
		VulkanEngine();
		~VulkanEngine();

		void Init();
		void Run();
		void Draw();
		void Cleanup();

		// sdl window creation
		void CreateWindow();

		// vulkan initialization
		void InitVulkan();
		void InitSwapchain();
		void CreateSwapChain(uint32_t width, uint32_t height);
		void DestroySwapChain();
		void InitCommands();
		void InitSyncStructures();

	private:
		bool       m_isInitialized;
		int        m_frameNumber;
		bool       m_stopRendering;
		VkExtent2D m_windowExtent;

		// sdl window stuff
		SDL_Window *m_window;

		// vulkan stuff
		VkInstance m_instance;
		VkDebugUtilsMessengerEXT m_debugMessenger;
		VkPhysicalDevice m_physicalDevice;
		VkDevice m_device;
		VkSurfaceKHR m_surface;

		// swapchain stuff
		VkSwapchainKHR m_swapChain;
		VkFormat m_swapChainImageFormat;
		std::vector<VkImage> m_swapChainImages;
		std::vector<VkImageView> m_swapChainImageViews;
		VkExtent2D m_swapChainExtent;
	};
}