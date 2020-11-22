#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utils.h"

struct UboModel {
	glm::mat4 model;

};

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice newPhysDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferPool,
		std::vector<Vertex>* verticies, std::vector<uint32_t>* indices);

	~Mesh() = default;
	
	void setModel(glm::mat4 newModel) { uboModel.model = newModel; };
	UboModel getModel() const { return uboModel; }

	uint32_t getVertexCount() const { return vertexCount; };
	VkBuffer getVertexBuffer() const { return vertexBuffer; };

	void destroyBuffers();

	VkBuffer getIndexBuffer() const { return indexBuffer; };
	uint32_t getIndexCount() const { return indexCount; };
private:
	UboModel uboModel;

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

