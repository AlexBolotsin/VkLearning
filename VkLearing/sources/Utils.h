#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm.hpp>

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 col;
};

struct QueueFalimyIndices {
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentationModes;
};

struct SwapchainImage {
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		throw std::runtime_error("Could not open file");
	}

	size_t filesize = (size_t)file.tellg();
	std::vector<char> fileBuffer(filesize);

	file.seekg(0);

	file.read(fileBuffer.data(), filesize);

	file.close();

	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physDevice, uint32_t allowedTypes, VkMemoryPropertyFlags memFlags)
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

static void createBuffer(VkPhysicalDevice physDevice, VkDevice device, VkDeviceSize bufferSize,
	VkBufferUsageFlags bufferUsage,	VkMemoryPropertyFlags bufferProps, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = bufferSize;
	bufferCreateInfo.usage = bufferUsage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed creating buffer");
	}

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(device, *buffer, &memReqs);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memReqs.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physDevice, memReqs.memoryTypeBits, bufferProps);

	if (vkAllocateMemory(device, &memoryAllocInfo, nullptr, bufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory");
	}

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	VkCommandBuffer transferCommandBuffer;

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbAllocInfo.commandPool = commandPool;
	cbAllocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device, &cbAllocInfo, &transferCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffer");
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	return transferCommandBuffer;
}

static VkCommandBuffer endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer comandBuffer)
{
	vkEndCommandBuffer(comandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &comandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &comandBuffer);

	return comandBuffer;
}


static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer,
	VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	VkBufferImageCopy imageRegion = {};
	imageRegion.bufferOffset = 0;
	imageRegion.bufferRowLength = 0;
	imageRegion.bufferImageHeight = 0;
	imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageRegion.imageSubresource.mipLevel = 0;
	imageRegion.imageSubresource.baseArrayLayer = 0;
	imageRegion.imageSubresource.layerCount = 1;
	imageRegion.imageOffset = { 0, 0, 0 };
	imageRegion.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	VkImageMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = oldLayout;
	memoryBarrier.newLayout = newLayout;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = image;
	memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memoryBarrier.subresourceRange.baseMipLevel = 0;
	memoryBarrier.subresourceRange.levelCount = 1;
	memoryBarrier.subresourceRange.baseArrayLayer = 0;
	memoryBarrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = 0;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}