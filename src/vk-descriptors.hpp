#pragma once

#include <vk-types.hpp>

namespace vr {
	class DescriptorLayoutBuilder final {
	public:
		void AddBinding(uint32_t binding, VkDescriptorType type);
		void Clear();

		VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages);

	private:
		std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	};
}