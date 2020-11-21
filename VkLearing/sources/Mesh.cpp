#include "Mesh.h"

Mesh::Mesh(VkPhysicalDevice newPhysDevice, VkDevice newDevice, VkQueue transferQueue,
	VkCommandPool transferPool, std::vector<Vertex>* verticies, std::vector<uint32_t>* indices)
{
	vertexCount = verticies->size();
	indexCount = indices->size();
	physDevice = newPhysDevice;
	device = newDevice;
	createVertexBuffer(transferQueue, transferPool, verticies);
	createIndexBuffer(transferQueue, transferPool, indices);
}

void Mesh::destroyBuffers()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMem, nullptr);
	vkFreeMemory(device, indexBufferMem, nullptr);
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferPool, std::vector<Vertex>* vertecies)
{
	VkDeviceSize bufferSize = sizeof(Vertex) * vertecies->size();

	//createStageBuffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(physDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertecies->data(), bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(physDevice, device, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&vertexBuffer, &vertexBufferMem);

	copyBuffer(device, transferQueue, transferPool, stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferPool, std::vector<uint32_t>* indices)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	//createStageBuffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(physDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(physDevice, device, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&indexBuffer, &indexBufferMem);

	copyBuffer(device, transferQueue, transferPool, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}
