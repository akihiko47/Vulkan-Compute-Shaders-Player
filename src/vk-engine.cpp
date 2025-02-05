#include "vk-engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk-initializers.hpp>
#include <vk-types.hpp>

#include <chrono>
#include <thread>


using namespace vr;


VulkanEngine::VulkanEngine() {
	Init();
}


VulkanEngine::~VulkanEngine() {
	Cleanup();
}


void VulkanEngine::Init() {
	std::cout << "Vulkan engine init\n";

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

	m_isInitialized = true;
}


void VulkanEngine::Cleanup() {
	std::cout << "Vulkan engine cleanup\n";

	if (m_isInitialized) {
		SDL_DestroyWindow(m_window);
	}
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
	std::cout << "Vulkan engine draw\n";
}