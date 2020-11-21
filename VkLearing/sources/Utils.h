#pragma once

#include <fstream>
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