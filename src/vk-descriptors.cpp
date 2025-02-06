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