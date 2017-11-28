#pragma once
#include "Mesh.h"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>

#include "VulkanBuffer.h"


Mesh::Mesh(const VulkanContextInfo& contextInfo, const MESHTYPE meshtype) {
	if (meshtype == MESHTYPE::NDCTRIANGLE) createNDCTriangle(contextInfo);
	if (meshtype == MESHTYPE::NDCBARRELMESH) createNDCBarrelMesh(contextInfo);
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, 
	const std::vector<Texture>& textures, const VulkanContextInfo& contextInfo)
	: mVertices(vertices), mIndices(indices), mTextures(textures)
{
	setupVulkanBuffers(contextInfo);
}

Mesh::Mesh() {
}

Mesh::~Mesh() {
}

void Mesh::createDescriptor(const VulkanContextInfo& contextInfo, const VkBuffer& ubo, const uint32_t sizeofUBOstruct) {
	//bind textures for this mesh
	if (mTextures.size() > 4) {
		std::cout << "\nMore than 4 textures in mesh" << std::endl;
	}
	for (unsigned int i = 0; i < mTextures.size(); ++i) {
		std::string name = mTextures[i].type;
		if (name == "texture_diffuse") {
			diffuseindices.push_back(i);
		} else if (name == "texture_specular") {
			specindices.push_back(i);
		} else if (name == "texture_normal") {
			norindices.push_back(i);
		} else if (name == "texture_height") {
			heightindices.push_back(i);
		} else {
			std::cout << "Not sure which texture type this is: " << name << std::endl;
		}
	}

	descriptor.determineNumImageSamplersAndTextureMapFlags(this);
	//VulkanDescriptor has static members for the differnt layouts which get created at init
	//the set is created based on the type
	descriptor.createDescriptorSetLayout(contextInfo);
	descriptor.createDescriptorPool(contextInfo);//each should have their own pool
	descriptor.createDescriptorSet(contextInfo,ubo,sizeofUBOstruct,this);

}


void Mesh::setupVulkanBuffers(const VulkanContextInfo& contextInfo) {
	//setup vulkan buffers for the geometry
	VulkanBuffer::createVertexBuffer(contextInfo, mVertices, vertexBuffer, vertexBufferMemory);
	VulkanBuffer::createIndexBuffer(contextInfo, mIndices, indexBuffer, indexBufferMemory);
}

void Mesh::createNDCTriangle(const VulkanContextInfo& contextInfo) {

	//top left (vulk top left of screen is -1,-1 and uv 0, 0) also the front face is set to ccw
	Vertex tempVertexInfo;
	tempVertexInfo.pos = glm::vec3(-1.0f, -1.0f, 0.5f);
	tempVertexInfo.color = glm::vec3(1.0f);
	tempVertexInfo.nor = glm::vec3(1.0f);
	tempVertexInfo.uv = glm::vec2(0.0f, 0.0f);
	mVertices.push_back(tempVertexInfo);
	mIndices.push_back(0);

	//bottom left screen below screen
	tempVertexInfo.pos = glm::vec3(-1.0f, 3.0f, 0.5f);
	tempVertexInfo.color = glm::vec3(1.0f);
	tempVertexInfo.nor = glm::vec3(1.0f);
	tempVertexInfo.uv = glm::vec2(0.0f, 2.0f);
	mVertices.push_back(tempVertexInfo);
	mIndices.push_back(1);

	//top right off screen
	tempVertexInfo.pos = glm::vec3(3.0f, -1.0f, 0.5f);
	tempVertexInfo.color = glm::vec3(1.0f);
	tempVertexInfo.nor = glm::vec3(1.0f);
	tempVertexInfo.uv = glm::vec2(2.0f, 0.0f);
	mVertices.push_back(tempVertexInfo);
	mIndices.push_back(2);

	VulkanBuffer::createVertexBuffer(contextInfo, mVertices, vertexBuffer, vertexBufferMemory);
	VulkanBuffer::createIndexBuffer(contextInfo, mIndices, indexBuffer, indexBufferMemory);
}

void Mesh::createNDCBarrelMesh(const VulkanContextInfo& contextInfo) {
	//top left (vulk top left of screen is -1,-1 and uv 0, 0) also the front face is set to ccw

	const uint32_t quadsPerDim = 30;//20?
	const float stride = 2.f / quadsPerDim;
	
	//vertices
	mVertices.reserve((quadsPerDim+1)*2);
	const glm::vec3 color(1.0f);
	const glm::vec3 nor(1.0f);
	for (float y = -1.f; y < 1.f+stride*0.5f; y += stride) {
		for (float x = -1.f; x < 1.f+stride*0.5f; x += stride) {
			Vertex v;
			v.pos = glm::vec3(x, y, 0.5f);
			v.uv = glm::vec2((x+1.f)*0.5f, (y+1.f)*0.5f);
			mVertices.push_back(v);
		}
	}

	//indexing
	mIndices.reserve(quadsPerDim*quadsPerDim*2*3);
	uint32_t first, second, third;
	for (uint32_t y = 0; y < quadsPerDim; ++y) {
		for (uint32_t x = 0; x < quadsPerDim; ++x) {
			first = y*(quadsPerDim+1) + x;
			second = first + 1 + quadsPerDim + 1;
			third = first + 1;
			mIndices.push_back(first);
			mIndices.push_back(second);
			mIndices.push_back(third);

			first = first + quadsPerDim + 1;
			second = first + 1;
			third = second - 1 - quadsPerDim - 1;
			mIndices.push_back(first);
			mIndices.push_back(second);
			mIndices.push_back(third);
		}
	}

	VulkanBuffer::createVertexBuffer(contextInfo, mVertices, vertexBuffer, vertexBufferMemory);
	VulkanBuffer::createIndexBuffer(contextInfo, mIndices, indexBuffer, indexBufferMemory);
}
