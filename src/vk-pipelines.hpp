#pragma once

#include <fstream>

#include <vk-initializers.hpp>


namespace vkutils {
	bool LoadShaderModule(const char *filePath, VkDevice device, VkShaderModule *outShaderModule) {
		
		// open file with cursor at the end
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			return false;
		}

		// get file size
		size_t fileSize = static_cast<size_t>(file.tellg());

		// reserve buffer of uints (for SPIRV)
		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

		// put cursor at beginning
		file.seekg(0);

		// load entire file into buffer
		file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

		// close file
		file.close();

		// create shader module
		VkShaderModuleCreateInfo moduleInfo{};
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.codeSize = buffer.size() * sizeof(uint32_t);
		moduleInfo.pCode = buffer.data();

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule)) {
			return false;
		}

		*outShaderModule = shaderModule;
		return true;
	}
}
