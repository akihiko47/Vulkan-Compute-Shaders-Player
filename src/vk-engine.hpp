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

	private:
		bool m_isInitialized;
		int m_frameNumber;
		bool m_stopRendering;
		VkExtent2D m_windowExtent;

		SDL_Window *m_window;
	};
}