#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#include <glfw/glfw3.h>
#include <vector>
#include <set>

#include "Utils.h"

class VkRenderer
{
public:
	VkRenderer();
	~VkRenderer();

	int Init(GLFWwindow* window);
	void Cleanup();

private:
	GLFWwindow* _window;

	VkInstance instance;
	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	std::vector<SwapchainImage> swapChainImages;

	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	void CreateInstance();

	void RetrievePhysicalDevice();
	void RetrieveLogicalDevice();
	void CrateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();
	void createRenderPass();
	void createGraphicsPipeline();

	//// Support
	/// Checkers
	bool CheckInstanceExtensionSupport(std::vector<const char*>& extensionsToBeChecked);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckDevice(VkPhysicalDevice device);

	/// Getters
	QueueFalimyIndices getQueueFamilies(VkPhysicalDevice device) const;
	SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device) const;

	/// choose functions
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	/// create
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);
};

