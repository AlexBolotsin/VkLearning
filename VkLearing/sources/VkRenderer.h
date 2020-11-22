#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#include <glfw/glfw3.h>
#include <vector>
#include <set>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "Utils.h"
#include "Mesh.h"

class VkRenderer
{
public:
	VkRenderer();
	~VkRenderer();

	int Init(GLFWwindow* window);

	void updateModel(glm::mat4 newModel);
	void Draw();
	void Cleanup();

private:
	GLFWwindow* _window;
	int currentFrame = 0;

	// scene objects
	std::vector<Mesh> meshes;

	// scene settings
	struct MVP
	{
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
	} mvp;

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
	std::vector<VkFramebuffer> swapChainFrameBuffer;
	std::vector<VkCommandBuffer> commandBuffers;

	// descriptors
	VkDescriptorSetLayout descSetLayout;
	VkDescriptorPool descPool;
	std::vector<VkDescriptorSet> descSets;

	std::vector<VkBuffer> uniformBuffer;
	std::vector<VkDeviceMemory> uniformBufferMem;

	//pipeline
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	//pools
	VkCommandPool graphicsCommandPool;

	//util
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	//synch
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

	void CreateInstance();

	void RetrievePhysicalDevice();
	void CrateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();
	void createRenderPass();
	void createDescriptoSetLayout();
	void createGraphicsPipeline();
	void createFramebuffer();
	void createGraphicsCommandPool();
	void createCommandBuffer();
	void createSynchronization();

	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffer(int index);

	// record function
	void recordCommands();

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

