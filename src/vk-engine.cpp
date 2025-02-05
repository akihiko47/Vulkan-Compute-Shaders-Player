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
}

void VulkanEngine::Cleanup() {
	std::cout << "Vulkan engine cleanup\n";
}