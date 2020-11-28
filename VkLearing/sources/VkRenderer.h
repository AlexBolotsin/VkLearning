#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#include <glfw/glfw3.h>
#include <vector>
#include <set>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "stb_image.h"

#include "Utils.h"
#include "Mesh.h"

class VkRenderer
{
public:
	VkRenderer();
	~VkRenderer();

	int Init(GLFWwindow* window);

	void updateModel(int modelIndex, glm::mat4 newModel);
	void Draw();
	void Cleanup();

private:
	GLFWwindow* _window;
	int currentFrame = 0;

	// scene objects
	std::vector<Mesh> meshes;

	// scene settings
	struct UboViewProjection
	{
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;

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

	VkImage depthBufferImage;
	VkDeviceMemory depthBufferImageMem;
	VkImageView depthBufferImageView;
	VkFormat depthFormat;

	// descriptors
	VkDescriptorSetLayout descSetLayout;
	VkPushConstantRange pushConstantRange;

	VkDescriptorPool descPool;
	std::vector<VkDescriptorSet> descSets;

	std::vector<VkBuffer> vpUniformBuffer;
	std::vector<VkDeviceMemory> vpUniformBufferMem;

	std::vector<VkBuffer> modelDynUniformBuffer;
	std::vector<VkDeviceMemory> modelDynUniformBufferMem;

	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory;

	//VkDeviceSize minUniformBufferOffset;
	//size_t modelUniformAllignment;
	//Model* modelTransferSpace;

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
	void createDescriptorSetLayout();
	void createPushConstantRange();
	void createGraphicsPipeline();
	void createDepthBuffeImage();
	void createFramebuffer();
	void createGraphicsCommandPool();
	void createCommandBuffer();
	void createSynchronization();

	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffers(int index);

	// record function
	void recordCommands(uint32_t index);

	//void allocateDynamicBufferTransferSpace();

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
	VkFormat chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

	/// create
	VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory *imageMem);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);

	int createTexture(std::string filename);

	// loaders
	stbi_uc* loadTextureFile(std::string filename, int* width, int* height, VkDeviceSize* imageSize);
};

