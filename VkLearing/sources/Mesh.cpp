#include "Mesh.h"

Mesh::Mesh(VkPhysicalDevice newPhysDevice, VkDevice newDevice, std::vector<Vertex>* verticies)
{
	vertexCount = verticies->size();
	physDevice = newPhysDevice;
	device = newDevice;
	createVertexBuffer(verticies);
}

void Mesh::destroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMem, nullptr);
}

void Mesh::createVertexBuffer(std::vector<Vertex>* vertecies)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = sizeof(Vertex) * vertecies->size();
	bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertexBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed creating buffer");
	}

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memReqs);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memReqs.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(device, &memoryAllocInfo, nullptr, &vertexBufferMem) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory");
	}

	vkBindBufferMemory(device, vertexBuffer, vertexBufferMem, 0);

	void* data;
	vkMapMemory(device, vertexBufferMem, 0, bufferCreateInfo.size, 0, &data);
	memcpy(data, vertecies->data(), bufferCreateInfo.size);
	vkUnmapMemory(device, vertexBufferMem);
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags memFlags)
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
	
	for (int i = 0; i < memProps.memoryTypeCount; ++i)
	{
		if ((allowedTypes & 1 << i) && (memProps.memoryTypes[i].propertyFlags & memFlags) == memFlags)
		{
			return i;
		}
	}
	return 0;
}

