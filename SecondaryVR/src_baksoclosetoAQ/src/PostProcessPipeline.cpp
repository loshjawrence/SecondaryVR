#pragma once
#include "PostProcessPipeline.h"

#include "Model.h"
#include "Utils.h"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>


PostProcessPipeline::PostProcessPipeline() {


}

PostProcessPipeline::PostProcessPipeline(const std::vector<std::string>& shaderpaths,
	const VulkanRenderPass& renderPass, const VulkanContextInfo& contextInfo, 
	const VkDescriptorSetLayout* setLayouts, const bool isPresent) 
	: shaderpaths(shaderpaths), isPresent(isPresent)
{

	//TODO: determine renderPass type here based on pipeline type? or will it all be one renderpass in the end?
	createPipeline(renderPass, contextInfo, setLayouts);

	addCommandPools(contextInfo, 1);

	allocateCommandBuffers(contextInfo);
	createSemaphores(contextInfo);

	createOutputImages(contextInfo);
	createFramebuffers(contextInfo, renderPass);
}


PostProcessPipeline::~PostProcessPipeline() {
}

void PostProcessPipeline::createOutputImages(const VulkanContextInfo& contextInfo) {
	outputImages.resize((contextInfo.camera.vrmode && !isPresent) ? contextInfo.camera.numQualitySettings : 1);

	for (int k = 0; k < outputImages.size(); ++k) {
		outputImages[k].resize(contextInfo.swapChainImages.size());
		for (int i = 0; i < contextInfo.swapChainImages.size(); ++i) {

			//TODO: if flag is present then use swapchain stuff otherwise 16F
			if (!isPresent) {
				VkExtent2D renderTargetExtent = contextInfo.camera.vrmode ? contextInfo.camera.renderTargetExtent[k] : contextInfo.camera.renderTargetExtentNoVR;
				outputImages[k][i] = VulkanImage(IMAGETYPE::COLOR_ATTACHMENT, renderTargetExtent, VK_FORMAT_R16G16B16A16_SFLOAT, contextInfo);
			} else {
				outputImages[k][i].image = contextInfo.swapChainImages[i];
				outputImages[k][i].imageView = contextInfo.swapChainImageViews[i];
				outputImages[k][i].extent = contextInfo.swapChainExtent;
			}
		}
	}
}

void PostProcessPipeline::addCommandPools(const VulkanContextInfo& contextInfo, const uint32_t num) {
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = contextInfo.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	for (int i = 0; i < num; ++i) {
		VkCommandPool commandPool;
		commandPools.push_back(commandPool);

		if (vkCreateCommandPool(contextInfo.device, &poolInfo, nullptr, &commandPools.back()) != VK_SUCCESS) {
			std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create graphics command pool!";
			throw std::runtime_error(ss.str());
		}
	}
}

void PostProcessPipeline::createFramebuffers(const VulkanContextInfo& contextInfo, const VulkanRenderPass& renderPass) {

	framebuffers.resize((contextInfo.camera.vrmode && !isPresent) ? contextInfo.camera.numQualitySettings : 1);

	for (int k = 0; k < framebuffers.size(); ++k) {//adaptive quality
		framebuffers[k].resize(contextInfo.swapChainImages.size());
		VkExtent2D renderTargetExtent = contextInfo.camera.vrmode ? contextInfo.camera.renderTargetExtent[k] : contextInfo.camera.renderTargetExtentNoVR;
		if (isPresent) {
			for (int i = 0; i < contextInfo.swapChainImages.size(); ++i) {
				framebuffers[k][i] = contextInfo.swapChainFramebuffers[i];
			}
		} else {
			for (int i = 0; i < contextInfo.swapChainImages.size(); ++i) {
				VkFramebufferCreateInfo framebufferCreateInfo = {};
				framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferCreateInfo.pNext = NULL;

				//TODO: if last make present otherwise normal post process
				framebufferCreateInfo.renderPass = renderPass.renderPassPostProcess;
				framebufferCreateInfo.pAttachments = &outputImages[k][i].imageView;
				framebufferCreateInfo.attachmentCount = 1;

				framebufferCreateInfo.width = renderTargetExtent.width;
				framebufferCreateInfo.height = renderTargetExtent.height;
				framebufferCreateInfo.layers = 1;

				if (vkCreateFramebuffer(contextInfo.device, &framebufferCreateInfo, nullptr, &framebuffers[k][i]) != VK_SUCCESS) {
					std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create framebuffer!";
					throw std::runtime_error(ss.str());
				}
			}
		}
	}
}

void PostProcessPipeline::allocateCommandBuffers(const VulkanContextInfo& contextInfo) {

	commandBuffers.resize((contextInfo.camera.vrmode && !isPresent) ? contextInfo.camera.numQualitySettings : 1);
	for (int k = 0; k < commandBuffers.size(); ++k) {
		commandBuffers[k].resize(contextInfo.swapChainFramebuffers.size());
		//clean up before record? no need, state is not maintained
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPools[0];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		//allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocInfo.commandBufferCount = commandBuffers.size();

		if (vkAllocateCommandBuffers(contextInfo.device, &allocInfo, commandBuffers[k].data()) != VK_SUCCESS) {
			std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to alloc pipeline command buffers!";
			throw std::runtime_error(ss.str());
		}
	}
}

void PostProcessPipeline::createPipeline(const VulkanRenderPass& renderPass,
	const VulkanContextInfo& contextInfo, const VkDescriptorSetLayout* setLayouts) {
	auto vertShaderCode = readFile(shaderpaths[0]);
	auto fragShaderCode = readFile(shaderpaths[1]);

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, contextInfo);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, contextInfo);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	//TODO: for dynamic amount of shaders, turn into vector pass .data to the pipeline info at the bottom
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = isPresent ? (float)contextInfo.swapChainExtent.width : contextInfo.camera.width;
	viewport.height = isPresent ? (float)contextInfo.swapChainExtent.height : contextInfo.camera.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	const int k = contextInfo.camera.qualityIndex;
	VkExtent2D renderTargetExtent = contextInfo.camera.vrmode ? contextInfo.camera.renderTargetExtent[k] : contextInfo.camera.renderTargetExtentNoVR;
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = isPresent ? contextInfo.swapChainExtent : renderTargetExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
	rasterizer.lineWidth = 1.0f;
	//rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	//////////////////////////////
	//// PUSH CONSTANT RANGES ////
	//////////////////////////////
	std::vector<VkPushConstantRange> pushContantRanges;
	VkPushConstantRange push1 = {};
	push1.offset = 0;
	push1.size = sizeof(PostProcessPushConstant);
	push1.stageFlags = PostProcessPushConstant::stages;
	pushContantRanges.push_back(push1);


	///////////////////////
	//// DYNAMIC STATE ////
	///////////////////////
	VkPipelineDynamicStateCreateInfo dynamicInfo = {};
	std::vector<VkDynamicState> dynamicStates;
	dynamicStates.push_back( VK_DYNAMIC_STATE_VIEWPORT );
	dynamicStates.push_back( VK_DYNAMIC_STATE_SCISSOR );
	//dynamicStates.push_back( VK_DYNAMIC_STATE_STENCIL_REFERENCE );//also: compare_mask?
	dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicInfo.pNext = nullptr;
	dynamicInfo.flags = 0;
	dynamicInfo.dynamicStateCount = dynamicStates.size();
	dynamicInfo.pDynamicStates = &dynamicStates[0];


	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = setLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = pushContantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushContantRanges.data();

	if (vkCreatePipelineLayout(contextInfo.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create pipeline layout!";
		throw std::runtime_error(ss.str());
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = isPresent ? renderPass.renderPassPostProcessPresent : renderPass.renderPassPostProcess;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.pDynamicState = &dynamicInfo;

	if (vkCreateGraphicsPipelines(contextInfo.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create graphics pipeline!";
		throw std::runtime_error(ss.str());
	}

	vkDestroyShaderModule(contextInfo.device, fragShaderModule, nullptr);
	vkDestroyShaderModule(contextInfo.device, vertShaderModule, nullptr);
}

VkShaderModule PostProcessPipeline::createShaderModule( const std::vector<char>& code, 
	const VulkanContextInfo& contextInfo) const 
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(contextInfo.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create shader module!";
		throw std::runtime_error(ss.str());
	}

	return shaderModule;
}

void PostProcessPipeline::getViewportAndScissor(VkViewport& outViewport, VkRect2D& outScissor, 
	const VulkanContextInfo& contextInfo, const uint32_t camIndex, const bool vrmode, const VkExtent2D& renderTargetExtent) {
	outViewport.minDepth = 0.0f;
	outViewport.maxDepth = 1.0f;

	//const float width = isPresent ? 
	//	(contextInfo.camera.vrmode ? contextInfo.swapChainExtent.width * 0.5f : contextInfo.swapChainExtent.width)
	//	: contextInfo.camera.width;
	//const float height = isPresent ? contextInfo.swapChainExtent.height : contextInfo.camera.height;
	const float width = contextInfo.camera.vrmode ? renderTargetExtent.width * 0.5f : renderTargetExtent.width;
	const float height = renderTargetExtent.height;
	if (vrmode) {
		outViewport.width = width;
		outViewport.height = height;
		outViewport.x = (0 == camIndex) ? 0.0f : outViewport.width;
		outViewport.y = 0.0f;
		outScissor.offset = { (int32_t)outViewport.x,		(int32_t)outViewport.y };
		outScissor.extent = { (uint32_t)outViewport.width,	(uint32_t)outViewport.height };
	} else {
		outViewport.x = 0.0f;
		outViewport.y = 0.0f;
		outViewport.width = width;
		outViewport.height = height;
		outScissor.offset = { 0, 0 };
		outScissor.extent = { (uint32_t)outViewport.width,	(uint32_t)outViewport.height };
	}
}

void PostProcessPipeline::createStaticCommandBuffers(const VulkanContextInfo& contextInfo, 
	const VulkanRenderPass& renderPass, const std::vector<Mesh>& meshes) 
{
	for (int k = 0; k < commandBuffers.size(); ++k) {
		for (int i = 0; i < contextInfo.swapChainImages.size(); ++i) {
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = commandPools[0];
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			//allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			allocInfo.commandBufferCount = 1;

			if (vkAllocateCommandBuffers(contextInfo.device, &allocInfo, &commandBuffers[k][i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to allocate command buffers!");
			}

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			//beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
			//beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr; // Optional


			vkBeginCommandBuffer(commandBuffers[k][i], &beginInfo);

			std::array<VkClearValue, 1> clearValues = {};
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
			//clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = isPresent ? renderPass.renderPassPostProcessPresent : renderPass.renderPassPostProcess;
			renderPassInfo.framebuffer = framebuffers[k][i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			VkExtent2D renderTargetExtent = contextInfo.camera.vrmode ? contextInfo.camera.renderTargetExtent[k] : contextInfo.camera.renderTargetExtentNoVR;
			renderTargetExtent = isPresent ? contextInfo.swapChainExtent : renderTargetExtent;
			renderPassInfo.renderArea.extent = renderTargetExtent;

			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(commandBuffers[k][i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[k][i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);


			vkCmdBindDescriptorSets(commandBuffers[k][i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &(inputDescriptors[k][i].descriptorSet), 0, nullptr);

			//VkBuffer vertexBuffers[] = { mesh.vertexBuffer };
			//VkBuffer indexBuffer = mesh.indexBuffer;
			VkBuffer vertexBuffers[] = { meshes[0].vertexBuffer };
			VkBuffer indexBuffer = meshes[0].indexBuffer;
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindVertexBuffers(commandBuffers[k][i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffers[k][i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);


			const uint32_t camIndex = 0;
			const PostProcessPushConstant pushconstant = { camIndex << 1 | static_cast<uint32_t>(contextInfo.camera.vrmode),
															renderTargetExtent.width, renderTargetExtent.height };
			vkCmdPushConstants(commandBuffers[k][i], pipelineLayout, PostProcessPushConstant::stages, 0, sizeof(PostProcessPushConstant), (const void*)&pushconstant);

			VkViewport viewport = {}; VkRect2D scissor = {};
			getViewportAndScissor(viewport, scissor, contextInfo, camIndex, contextInfo.camera.vrmode, renderTargetExtent);
			vkCmdSetViewport(commandBuffers[k][i], 0, 1, &viewport);
			vkCmdSetScissor(commandBuffers[k][i], 0, 1, &scissor);

			//vkCmdDrawIndexed(commandBuffers[k][i], static_cast<uint32_t>(mesh.mIndices.size()), 1, 0, 0, 0);
			vkCmdDrawIndexed(commandBuffers[k][i], static_cast<uint32_t>(meshes[0].mIndices.size()), 1, 0, 0, 0);

			if (contextInfo.camera.vrmode) {
				//bind other precalc mesh(can't just use the same one since not the same(asymmetrical/mirrored)
				const uint32_t camIndex = 1;
				VkBuffer vertexBuffers[] = { meshes[camIndex].vertexBuffer };
				VkBuffer indexBuffer = meshes[camIndex].indexBuffer;
				VkDeviceSize offsets[] = { 0 };

				vkCmdBindVertexBuffers(commandBuffers[k][i], 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffers[k][i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

				const PostProcessPushConstant pushconstant = { camIndex << 1 | static_cast<uint32_t>(contextInfo.camera.vrmode),
																renderTargetExtent.width, renderTargetExtent.height };
				vkCmdPushConstants(commandBuffers[k][i], pipelineLayout, PostProcessPushConstant::stages, 0, sizeof(PostProcessPushConstant), (const void*)&pushconstant);
				getViewportAndScissor(viewport, scissor, contextInfo, camIndex, contextInfo.camera.vrmode, renderTargetExtent);
				vkCmdSetViewport(commandBuffers[k][i], 0, 1, &viewport);
				vkCmdSetScissor(commandBuffers[k][i], 0, 1, &scissor);

				//vkCmdDrawIndexed(commandBuffers[k][i], static_cast<uint32_t>(mesh.mIndices.size()), 1, 0, 0, 0);
				vkCmdDrawIndexed(commandBuffers[k][i], static_cast<uint32_t>(meshes[camIndex].mIndices.size()), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffers[k][i]);

			if (vkEndCommandBuffer(commandBuffers[k][i]) != VK_SUCCESS) {
				std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to record command buffer!";
				throw std::runtime_error(ss.str());
			}
		}//i
	}//k
}

void PostProcessPipeline::createInputDescriptors(const VulkanContextInfo& contextInfo, 
	const std::vector< std::vector<VulkanImage> >& vulkanImages)
{
	inputDescriptors.resize(vulkanImages.size());
	for (int k = 0; k < vulkanImages.size(); ++k) {//adaptive quality
		inputDescriptors[k].resize(contextInfo.swapChainImages.size());
		for (int i = 0; i < contextInfo.swapChainImages.size(); ++i) {
			inputDescriptors[k][i].numImageSamplers = 1;//TODO: determine num image samplers of previous stage from size of VulkanImage vector
			inputDescriptors[k][i].createDescriptorSetLayoutPostProcess(contextInfo);
			inputDescriptors[k][i].createDescriptorPoolPostProcess(contextInfo);

			//may want to extent this to include cases where a post process has multiple render targets and therefore VulkanImages
			const std::vector<VulkanImage>& vulkanImagesAtSwapIndex = { vulkanImages[k][i] };
			inputDescriptors[k][i].createDescriptorSetPostProcess(contextInfo, vulkanImagesAtSwapIndex);
		}
	}
}



void PostProcessPipeline::createSemaphores(const VulkanContextInfo& contextInfo) {
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(contextInfo.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(contextInfo.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) 
	{
		std::stringstream ss; ss << "\n" << __LINE__ << ": " << __FILE__ << ": failed to create semaphores!";
		throw std::runtime_error(ss.str());
	}
}

void PostProcessPipeline::destroyVulkanPipeline(const VulkanContextInfo& contextInfo) {
	freeCommandBuffers(contextInfo);
	destroyPipeline(contextInfo);
	destroyPipelineLayout(contextInfo);
}

void PostProcessPipeline::freeCommandBuffers(const VulkanContextInfo& contextInfo) {
	for (int k = 0; k < commandBuffers.size(); ++k) {
		vkFreeCommandBuffers(contextInfo.device, commandPools[0], static_cast<uint32_t>(commandBuffers[k].size()), commandBuffers[k].data());
	}
}

void PostProcessPipeline::destroyPipeline(const VulkanContextInfo& contextInfo) {
	vkDestroyPipeline(contextInfo.device, graphicsPipeline, nullptr);
}

void PostProcessPipeline::destroyPipelineLayout(const VulkanContextInfo& contextInfo) {
	vkDestroyPipelineLayout(contextInfo.device, pipelineLayout, nullptr);
}

void PostProcessPipeline::destroyPipelineSemaphores(const VulkanContextInfo& contextInfo) {
	vkDestroySemaphore(contextInfo.device, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(contextInfo.device, imageAvailableSemaphore, nullptr);
}

