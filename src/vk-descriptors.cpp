#include <vk-descriptors.hpp>

using namespace vr;

void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type) {
	VkDescriptorSetLayoutBinding newbind{};
	newbind.binding = binding;
	newbind.descriptorCount = 1;
	newbind.descriptorType = type;

	m_bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::Clear() {
	m_bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device, VkShaderStageFlags shaderStages) {
	for (auto &b : m_bindings) {
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pBindings = m_bindings.data();
	info.bindingCount = static_cast<uint32_t>(m_bindings.size());
	info.flags = 0;

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}


// Descriptor allocator

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout layout) {
	VkDescriptorSetAllocateInfo setInfo{};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setInfo.descriptorPool = m_pool;
	setInfo.descriptorSetCount = 1;
	setInfo.pSetLayouts = &layout;

	VkDescriptorSet set;
	VK_CHECK(vkAllocateDescriptorSets(device, &setInfo, &set));

	return set;
}


void DescriptorAllocator::InitPool(VkDevice device, uint32_t maxSets, const std::vector<PoolSizeRatio> &poolRatios) {
	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());
	for (auto &ratio : poolRatios) {
		VkDescriptorPoolSize size{};
		size.type = ratio.type;
		size.descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets);

		poolSizes.emplace_back(size);
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = maxSets;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool);
}


void DescriptorAllocator::ClearDescriptors(VkDevice device) {
	vkResetDescriptorPool(device, m_pool, 0);
}


void DescriptorAllocator::DestroyPool(VkDevice device) {
	vkDestroyDescriptorPool(device, m_pool, nullptr);
}