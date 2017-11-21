#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif // !GLFW_INCLUDE_VULKAN

#include "VulkanContextInfo.h"


//"A pipeline is always built relative to a specific subpass of a specific render pass. It cannot be used in any other subpass.
//wanting to use the same pipeline for multiple subpasses is not typical of the common use cases for subpasses. 
//If you're wanting to do this, it may be a signal that there is a simpler or more efficient way to do what you're trying to do. 
//Generally in multiple-subpass render passes, each subpass is performing a different sub-algorithm of the overall rendering algorithm, 
//so the shaders (and therefore pipelines) will be different."


class VulkanRenderPass {
public:
	VkRenderPass renderPass;
	

public:
	VulkanRenderPass();
	VulkanRenderPass(const VulkanContextInfo& contextInfo);
	~VulkanRenderPass();
	void createRenderPass(const VulkanContextInfo& contextInfo);

	//cleanup
	void destroyRenderPass(const VulkanContextInfo& contextInfo);
};

