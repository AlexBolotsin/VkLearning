#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utils.h"

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice newPhysDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferPool,
		std::vector<Vertex>* verticies, std::vector<uint32_t>* indices);

	~Mesh() = default;
	
	uint32_t getVertexCount() const { return vertexCount; };
	VkBuffer getVertexBuffer() const { return vertexBuffer; };

	void destroyBuffers();

	VkBuffer getIndexBuffer() const { return indexBuffer; };
	uint32_t getIndexCount() const { return indexCount; };
private:
	uint32_t vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMem;

	uint32_t indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMem;

	VkPhysicalDevice physDevice;
	VkDevice device;

	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferPool, std::vector<Vertex>* vertecies);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferPool, std::vector<uint32_t>* indices);
};

