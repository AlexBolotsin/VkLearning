#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utils.h"

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice newPhysDevice, VkDevice newDevice, std::vector<Vertex>* verticies);

	~Mesh() = default;

	
	int getVertexCount() const { return vertexCount; };
	VkBuffer getVertexBuffer() const { return vertexBuffer; };

	void destroyVertexBuffer();

private:
	int vertexCount;

	VkBuffer vertexBuffer;
	VkPhysicalDevice physDevice;
	VkDevice device;
	VkDeviceMemory vertexBufferMem;

	void createVertexBuffer(std::vector<Vertex>* vertecies);

	uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags memFlags);
};

