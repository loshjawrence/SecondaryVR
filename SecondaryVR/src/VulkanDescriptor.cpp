#include "VulkanDescriptor.h"

#include <array>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>

std::vector<VkDescriptorSetLayout> VulkanDescriptor::layoutTypes = 
std::vector<VkDescriptorSetLayout>(VulkanDescriptor::MAX_IMAGESAMPLERS + 1);

bool VulkanDescriptor::layoutsInitialized = false;

void initDescriptorSetLayoutTypes(const VulkanContextInfo& contextInfo) {
	VulkanDescriptor::layoutTypes.reserve(VulkanDescriptor::MAX_IMAGESAMPLERS + 1);
	std::vector<VkDescriptorSetLayoutBinding> bindings = {}; 

	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.pImmutableSamplers = nullptr;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;


	bindings.push_back(uboLayoutBinding);

	//Make a no-texture descriptor layout
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(contextInfo.device, &layoutInfo, nullptr, &VulkanDescriptor::layoutTypes[0]) != VK_SUCCESS) {
		std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create descriptor set layout!";
		throw std::runtime_error(ss.str());
	}

	//make various layouts using 1 to MAX_IMAGESAMPLERS number of image samplers
	//store them in the static vector layoutTypes
	for (uint32_t i = 1; i < VulkanDescriptor::MAX_IMAGESAMPLERS + 1; ++i) {
		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = i;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings.push_back(samplerLayoutBinding);

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(contextInfo.device, &layoutInfo, nullptr, &VulkanDescriptor::layoutTypes[i]) != VK_SUCCESS) {
			std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create descriptor set layout!";
			throw std::runtime_error(ss.str());
		}
	}

	VulkanDescriptor::layoutsInitialized = true;
}

VulkanDescriptor::VulkanDescriptor() {
}

VulkanDescriptor::VulkanDescriptor(const VulkanContextInfo& contextInfo)
{
	createDescriptorSetLayout(contextInfo);
	createDescriptorPool(contextInfo);
}

VulkanDescriptor::~VulkanDescriptor() {
} 

void VulkanDescriptor::createDescriptorSetLayout(const VulkanContextInfo& contextInfo) {
	if (VulkanDescriptor::layoutsInitialized == false) 
		initDescriptorSetLayoutTypes(contextInfo);

	descriptorSetLayout = VulkanDescriptor::layoutTypes[numImageSamplers];

	//VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	//uboLayoutBinding.binding = 0;
	//uboLayoutBinding.descriptorCount = 1;
	//uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	//uboLayoutBinding.pImmutableSamplers = nullptr;
	//uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	//samplerLayoutBinding.binding = 1;
	//samplerLayoutBinding.descriptorCount = 1;
	//samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//samplerLayoutBinding.pImmutableSamplers = nullptr;
	//samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	//std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
	//VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	//layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	//layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	//layoutInfo.pBindings = bindings.data();

	//if (vkCreateDescriptorSetLayout(contextInfo.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
	//	std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create descriptor set layout!";
	//	throw std::runtime_error(ss.str());
	//}
}
void VulkanDescriptor::createDescriptorPool(const VulkanContextInfo& contextInfo) {
	std::vector<VkDescriptorPoolSize> poolSizes(numImageSamplers+1);
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	for (int i = 1; i < numImageSamplers+1; ++i) {
		poolSizes[i].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[i].descriptorCount = 1;
	}

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(contextInfo.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create descriptor set pool!";
		throw std::runtime_error(ss.str());
	}
}

void VulkanDescriptor::createDescriptorSet(const VulkanContextInfo& contextInfo, const VkBuffer& uniformBuffer,
	const int sizeofUBOstruct, const VulkanImage& vulkanImage)
{
	VkDescriptorSetLayout layouts[] = { descriptorSetLayout };
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(contextInfo.device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set!");
	}

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeofUBOstruct;

	//TODO: make a vector and cycle through Texture vector to determine where they should go
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = vulkanImage.imageView;
	imageInfo.sampler = vulkanImage.sampler;

	//TODO: make vector size of numImageSamplers+1 and cycle through imageInfo above for descriptorWrites[1+]
	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = descriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(contextInfo.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanDescriptor::determineDescriptorType(const uint32_t diffuseSize, const uint32_t specSize,
	const uint32_t norSize, const uint32_t heightSize) {
	DescriptorType type;
	if (diffuseSize == 0) {
		type = DescriptorType::HAS_NONE;
		numImageSamplers = 0;
	} else if (diffuseSize > 0 && norSize == 0 && specSize == 0 && heightSize == 0) {
		type = DescriptorType::HAS_DIFFUSE;
		numImageSamplers = 1;
	} else if (diffuseSize > 0 && norSize > 0 && specSize == 0 && heightSize == 0) {
		type = DescriptorType::HAS_NOR;
		numImageSamplers = 2;
	} else if (diffuseSize > 0 && norSize > 0 && specSize > 0 && heightSize == 0) {
		type = DescriptorType::HAS_SPEC;
		numImageSamplers = 3;
	} else if (diffuseSize > 0 && norSize == 0 && specSize == 0 && heightSize > 0) {
		type = DescriptorType::HAS_HEIGHT;
		numImageSamplers = 3;
	} else if (diffuseSize > 0 && norSize  > 0 && specSize  > 0 && heightSize  > 0) {
		type = DescriptorType::HAS_ALL; 
		numImageSamplers = 4;
	} else {
		std::stringstream ss; ss << "\nLINE: " << __LINE__ << ": FILE: " << __FILE__ << ": failed to determine mesh's descriptor type";
		throw std::runtime_error(ss.str());
		type = DescriptorType::HAS_NONE;
		numImageSamplers = 0;
	}
}


void VulkanDescriptor::destroyVulkanDescriptor(const VulkanContextInfo& contextInfo) {
	destroyDescriptorPool(contextInfo);
	destroyDescriptorSetLayout(contextInfo);
}

void VulkanDescriptor::destroyDescriptorPool(const VulkanContextInfo& contextInfo) {
	vkDestroyDescriptorPool(contextInfo.device, descriptorPool, nullptr);
}

void VulkanDescriptor::destroyDescriptorSetLayout(const VulkanContextInfo& contextInfo) {
	if(descriptorSetLayout != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(contextInfo.device, descriptorSetLayout, nullptr);
}