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


	class DescriptorAllocator final {
	public:
		struct PoolSizeRatio {
			VkDescriptorType type;
			float ratio;
		};

		VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout);

		void InitPool(VkDevice device, uint32_t maxSets, const std::vector<PoolSizeRatio> &poolRatios);
		void ClearDescriptors(VkDevice device);
		void DestroyPool(VkDevice device);

	private:

		VkDescriptorPool m_pool;
	};
}