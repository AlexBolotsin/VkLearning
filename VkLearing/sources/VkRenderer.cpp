#include "VkRenderer.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <array>
#include <malloc.h>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 2;

VkRenderer::VkRenderer()
{

}

int VkRenderer::Init(GLFWwindow* window)
{
	_window = window;

	try {
		CreateInstance();

		CreateSurface();
		RetrievePhysicalDevice();
		CrateLogicalDevice();
		CreateSwapChain();
		createDepthBuffeImage();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffer();
		createGraphicsCommandPool();
		
		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		uboViewProjection.projection[1][1] *= -1;

		//Vertex data
		std::vector<Vertex> meshVertecies1 = {
			{{-0.4, 0.4, 0.0}, {0.1, 0.9, 0.6}},  // 0
			{{-0.4, -0.4, 0.0}, {0.4, 0.0, 0.5}},   // 1
			{{ 0.4, -0.4, 0.0}, {0.7, 0.1, 0.4}},  // 2
			{{ 0.4, 0.4, 0.0}, {0.3, 0.5, 0.1}}  // 3
		};

		std::vector<Vertex> meshVertecies2 = {
			{{ -0.25, 0.6, 0.0}, {0.1, 0.9, 0.6}},  // 0
			{{ -0.25, -0.6, 0.0}, {0.4, 0.0, 0.5}},   // 1
			{{ 0.25, -0.6, 0.0}, {0.7, 0.1, 0.4}},  // 2
			{{ 0.25, 0.6, 0.0}, {0.3, 0.5, 0.1}}  // 3
		};
		// Index data
		std::vector<uint32_t> meshIndices =
		{
			0, 1, 2,
			2, 3, 0
		};
		meshes = { Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, &meshVertecies1, &meshIndices),
			Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, &meshVertecies2, &meshIndices) };

		glm::mat4 meshModelMatrix = meshes[0].getModel().model;
		meshModelMatrix = glm::rotate(meshModelMatrix, glm::radians(45.f), glm::vec3(0.0f, 0.0f, 1.0f));
		meshes[0].setModel(meshModelMatrix);

		createCommandBuffer();
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createSynchronization();
	}
	catch (const std::runtime_error& e)
	{
		printf("Exception %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VkRenderer::updateModel(int modelIndex, glm::mat4 newModel)
{
	if (modelIndex >= meshes.size()) return;

	meshes[modelIndex].setModel(newModel);
}

void VkRenderer::Draw()
{
	// wait for given fence to signal open from last frame
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	// Get available image to draw and set something to signal when we're finished with the image
	uint32_t imageIndex = 0;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	recordCommands(imageIndex);
	updateUniformBuffers(imageIndex);

	// submit command bugger to queue for execution, making sure it waits for the image to be signaled as available before drawing
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed submit command buffer to queue");
	}
	
	// and signals whne it has finished rendering
	// present image to screen when it has signaled finished rendering
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	if (vkQueuePresentKHR(presentationQueue, &presentInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present image");
	}

	currentFrame = (currentFrame + 1) & MAX_FRAME_DRAWS;
}

void VkRenderer::Cleanup()
{
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMem, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descSetLayout, nullptr);

	for (int i = 0; i < swapChainImages.size(); ++i)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMem[i], nullptr);
// 		vkDestroyBuffer(mainDevice.logicalDevice, modelDynUniformBuffer[i], nullptr);
// 		vkFreeMemory(mainDevice.logicalDevice, modelDynUniformBufferMem[i], nullptr);
	}

	for (auto& mesh : meshes)
	{
		mesh.destroyBuffers();
	}

	for (int i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapChainFrameBuffer)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}

	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

	for (auto image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
}

VkRenderer::~VkRenderer()
{
}

void VkRenderer::CreateInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;


	std::vector<const char*> instanceExtensions;

	uint32_t extensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);


	for (uint32_t i = 0; i < extensionCount; ++i)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (!CheckInstanceExtensionSupport(instanceExtensions))
	{
		throw std::runtime_error("Some extensions are not supported");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = nullptr;

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create VK instance");
	}
}

void VkRenderer::RetrievePhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
		throw std::runtime_error("No GPU devices were found");

	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (CheckDevice(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

// 	VkPhysicalDeviceProperties deviceProperties;
// 	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

void VkRenderer::CrateLogicalDevice()
{
	QueueFalimyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndicies = { indices.graphicsFamily, indices.presentationFamily };

	for (int familyIndex : queueFamilyIndicies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
		queueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures physDeviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.pEnabledFeatures = &physDeviceFeatures;

	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create logical device");
	}

	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VkRenderer::CreateSurface()
{
	VkResult result = glfwCreateWindowSurface(instance, _window, nullptr, &surface);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create surface.");
	}
}

void VkRenderer::CreateSwapChain()
{
	SwapChainDetails swapChainDetails = GetSwapChainDetails(mainDevice.physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	uint32_t const maxImageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0
		&& swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.imageExtent = extent;
	swapchainCreateInfo.minImageCount = imageCount;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.surface = surface;

	QueueFalimyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};

		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed creating swapchain");
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);

	if (swapChainImageCount == 0)
	{
		throw std::runtime_error("Swapchain has no images");
	}

	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

	for (VkImage image : images)
	{
		SwapchainImage swapChainImage = {};
		swapChainImage.image = image;
		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		swapChainImages.push_back(swapChainImage);
	}
}

void VkRenderer::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	std::array<VkSubpassDependency, 2> subpassDeps;

	subpassDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDeps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDeps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	subpassDeps[0].dstSubpass = 0;
	subpassDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDeps[0].dependencyFlags = 0;

	subpassDeps[1].srcSubpass = 0;
	subpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				
	subpassDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDeps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDeps[1].dependencyFlags = 0;

	VkAttachmentDescription attachDesc[] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachDesc;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 2;
	renderPassCreateInfo.pDependencies = subpassDeps.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, NULL, &renderPass);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create renderpass");
	}
}

void VkRenderer::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	vpLayoutBinding.binding = 0;
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpLayoutBinding.descriptorCount = 1;
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vpLayoutBinding.pImmutableSamplers = nullptr;

// 	VkDescriptorSetLayoutBinding modelLayoutBinding = {};
// 	modelLayoutBinding.binding = 1;
// 	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
// 	modelLayoutBinding.descriptorCount = 1;
// 	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
// 	modelLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layoutBindings[] = { vpLayoutBinding/*, modelLayoutBinding*/ };

	VkDescriptorSetLayoutCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = 1;
	createInfo.pBindings = layoutBindings;

	if (vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &descSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("descriptorSetLayout has failed");
	}
}

void VkRenderer::createPushConstantRange()
{
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Model);
}

void VkRenderer::createGraphicsPipeline()
{
	auto vertexShaderCore = readFile("shaders/vert.spv");
	auto fragmentShaderCore = readFile("shaders/frag.spv");

	//build shader module
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCore);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCore);

	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.stride = sizeof(Vertex);
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 2> attrDesc = {};
	attrDesc[0].binding = 0;
	attrDesc[0].location = 0;
	attrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrDesc[0].offset = offsetof(Vertex, pos);

	attrDesc[1].binding = 0;
	attrDesc[1].location = 1;
	attrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrDesc[1].offset = offsetof(Vertex, col);

	// vertex input
	VkPipelineVertexInputStateCreateInfo vertextInputCreateInfo = {};
	vertextInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertextInputCreateInfo.pVertexBindingDescriptions = &bindingDesc;
	vertextInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertextInputCreateInfo.pVertexAttributeDescriptions = attrDesc.data();
	vertextInputCreateInfo.vertexAttributeDescriptionCount = attrDesc.size();

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo assemblyCreateInfo = {};
	assemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	assemblyCreateInfo.primitiveRestartEnable = false;

	// viewport
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0,0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;
	
	// -- DYNAMIC STATES --
	// Dynamic states to enable
	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);	// Dynamic Viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);	// Dynamic Scissor	: Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	//// Dynamic State creation info
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
	
	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
	rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationCreateInfo.lineWidth = 1.0f;
	rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorState.blendEnable = VK_TRUE;
	
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;

	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorblendingCreateInfo = {};
	colorblendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorblendingCreateInfo.logicOpEnable = VK_FALSE;
	colorblendingCreateInfo.attachmentCount = 1;
	colorblendingCreateInfo.pAttachments = &colorState;
	//colorblendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;

	createPushConstantRange();

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Error to create pipelineLayout");
	}

	// TODO: DEPTH STENCIL
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipelineCreateinfo = {};
	pipelineCreateinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateinfo.stageCount = 2;
	pipelineCreateinfo.pStages = shaderStages;
	pipelineCreateinfo.pVertexInputState = &vertextInputCreateInfo;
	pipelineCreateinfo.pInputAssemblyState = &assemblyCreateInfo;
	pipelineCreateinfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateinfo.pDynamicState = nullptr;
	pipelineCreateinfo.pRasterizationState = &rasterizationCreateInfo;
	pipelineCreateinfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateinfo.pColorBlendState = &colorblendingCreateInfo;
	pipelineCreateinfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineCreateinfo.layout = pipelineLayout;
	pipelineCreateinfo.renderPass = renderPass;
	pipelineCreateinfo.subpass = 0;

	pipelineCreateinfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateinfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateinfo, nullptr, &graphicsPipeline);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create graphics pipeline");
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VkRenderer::createDepthBuffeImage()
{
	depthFormat = chooseSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	depthBufferImage = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMem);

	depthBufferImageView = createImageView(depthBufferImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VkRenderer::createFramebuffer()
{
	swapChainFrameBuffer.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainFrameBuffer.size(); ++i)
	{
		std::array<VkImageView, 2> attachments = {
			swapChainImages[i].imageView,
			depthBufferImageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = swapChainExtent.width;
		framebufferCreateInfo.height = swapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFrameBuffer[i]);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer");
		}
	}
}

void VkRenderer::createGraphicsCommandPool()
{
	QueueFalimyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

	// create a graphics queue family command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("couldn't create command pool");
	}
}

void VkRenderer::createCommandBuffer()
{
	commandBuffers.resize(swapChainFrameBuffer.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffer");
	}
}

void VkRenderer::createSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// let's create semaphore
	VkSemaphoreCreateInfo semCreateInfo = {};
	semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a semaphore");
		}
	}
}

void VkRenderer::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(uboViewProjection);

	//VkDeviceSize modelBufferSize = modelUniformAllignment * MAX_OBJECTS;

	vpUniformBuffer.resize(swapChainImages.size());
	vpUniformBufferMem.resize(swapChainImages.size());
	//modelDynUniformBuffer.resize(swapChainImages.size());
	//modelDynUniformBufferMem.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMem[i]);

		//createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDynUniformBuffer[i], &modelDynUniformBufferMem[i]);
	}
}

void VkRenderer::createDescriptorPool()
{
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

// 	VkDescriptorPoolSize modelPoolSize = {};
// 	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
// 	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynUniformBuffer.size());

	VkDescriptorPoolSize descriptorPoolSizes[] = { vpPoolSize/*, modelPoolSize*/ };

	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());
	createInfo.poolSizeCount = 1;
	createInfo.pPoolSizes = descriptorPoolSizes;

	if (vkCreateDescriptorPool(mainDevice.logicalDevice, &createInfo, nullptr, &descPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create desc pool");
	}
}

void VkRenderer::createDescriptorSets()
{
	descSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = { };
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	allocInfo.pSetLayouts = setLayouts.data();

	if (vkAllocateDescriptorSets(mainDevice.logicalDevice, &allocInfo, descSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed allocating desc sets");
	}

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		VkDescriptorBufferInfo vpBufferInfo = {};
		vpBufferInfo.buffer = vpUniformBuffer[i];
		vpBufferInfo.offset = 0;
		vpBufferInfo.range = sizeof(uboViewProjection);

		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descSets[i];
		vpSetWrite.dstBinding = 0;
		vpSetWrite.dstArrayElement = 0;
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpSetWrite.descriptorCount = 1;
		vpSetWrite.pBufferInfo = &vpBufferInfo;

// 		VkDescriptorBufferInfo modelBufferInfo = {};
// 		modelBufferInfo.buffer = modelDynUniformBuffer[i];
// 		modelBufferInfo.offset = 0;
// 		modelBufferInfo.range = modelUniformAllignment;
// 
// 		VkWriteDescriptorSet modelSetWrite = {};
// 		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
// 		modelSetWrite.dstSet = descSets[i];
// 		modelSetWrite.dstBinding = 1;
// 		modelSetWrite.dstArrayElement = 0;
// 		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
// 		modelSetWrite.descriptorCount = 1;
// 		modelSetWrite.pBufferInfo = &modelBufferInfo;

		VkWriteDescriptorSet writeDescs[] = { vpSetWrite/*, modelSetWrite*/ };
		vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, writeDescs, 0, nullptr);
	}
}

void VkRenderer::updateUniformBuffers(int index)
{
	void* data = nullptr;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMem[index], 0, sizeof(uboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(uboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMem[index]);

// 	for (uint32_t i = 0; i < meshes.size(); ++i)
// 	{
// 		Model* model = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAllignment));
// 		*model = meshes[i].getModel();
// 	}

// 	vkMapMemory(mainDevice.logicalDevice, modelDynUniformBufferMem[index], 0, modelUniformAllignment * meshes.size(), 0, &data);
// 	memcpy(data, modelTransferSpace, modelUniformAllignment * meshes.size());
// 	vkUnmapMemory(mainDevice.logicalDevice, modelDynUniformBufferMem[index]);
}

void VkRenderer::recordCommands(uint32_t index)
{
	// how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginCommand = {};
	bufferBeginCommand.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkRenderPassBeginInfo renderpassBeginInfo = {};
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.renderPass = renderPass;
	renderpassBeginInfo.renderArea.offset = { 0, 0 };
	renderpassBeginInfo.renderArea.extent = swapChainExtent;
	
	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = { 0.6f, 0.65f, 0.4f, 1.0f };
	clearValues[1].depthStencil.depth = 1.0f;

	renderpassBeginInfo.pClearValues = clearValues.data();
	renderpassBeginInfo.clearValueCount = clearValues.size();

	renderpassBeginInfo.framebuffer = swapChainFrameBuffer[index];

	// start recording commands to command buffer
	if (vkBeginCommandBuffer(commandBuffers[index], &bufferBeginCommand) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to start recording a command buffer");
	}
	{
		vkCmdBeginRenderPass(commandBuffers[index], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdBindPipeline(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			int k = 0;
			for (auto& mesh : meshes)
			{
				VkBuffer vertexBuffer[] = { mesh.getVertexBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffers[index], 0, 1, vertexBuffer, offsets);

				vkCmdBindIndexBuffer(commandBuffers[index], mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				//uint32_t dynOffset = static_cast<uint32_t>(modelUniformAllignment) * k;

				Model model = mesh.getModel();
				vkCmdPushConstants(commandBuffers[index], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model), &model);

				vkCmdBindDescriptorSets(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSets[index], 0, nullptr);

				vkCmdDrawIndexed(commandBuffers[index], mesh.getIndexCount(), 1, 0, 0, 0);
				++k;
			}
		}
		vkCmdEndRenderPass(commandBuffers[index]);
	}

	if (vkEndCommandBuffer(commandBuffers[index]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to stop recording a command buffer");
	}
}

/*void VkRenderer::allocateDynamicBufferTransferSpace()
{
	modelUniformAllignment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

	modelTransferSpace = (Model*)_aligned_malloc(modelUniformAllignment * MAX_OBJECTS, modelUniformAllignment);
}*/

bool VkRenderer::CheckInstanceExtensionSupport(std::vector<const char*>& extensionsToBeChecked)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (auto& extName : extensionsToBeChecked)
	{
		auto itr = find_if(extensions.begin(), extensions.end(), [&extName](auto& ext) {
			return strcmp(extName, ext.extensionName) == 0 ? true : false;
			});
		if (itr == extensions.end())
			return false;
	}

	return true;
}

bool VkRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)
		return false;

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtensions = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtensions = true;
				break;
			}
		}

		if (!hasExtensions)
			return false;
	}

	return true;
}

bool VkRenderer::CheckDevice(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(device, &properties);

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(device, &features);

	QueueFalimyIndices indices = getQueueFamilies(device);

	bool extensionSupported = CheckDeviceExtensionSupport(device);
	bool swapchainSupported = false;

	if (extensionSupported)
	{
		SwapChainDetails swapchainDetails = GetSwapChainDetails(device);
		swapchainSupported = !swapchainDetails.presentationModes.empty() && !swapchainDetails.formats.empty();
	}

	return indices.isValid() && extensionSupported && swapchainSupported;
}

QueueFalimyIndices VkRenderer::getQueueFamilies(VkPhysicalDevice device) const
{
	QueueFalimyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyCont(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyCont.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilyCont)
	{
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);

		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.isValid())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapChainDetails VkRenderer::GetSwapChainDetails(VkPhysicalDevice device) const
{
	SwapChainDetails swapchainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapchainDetails.surfaceCapabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount > 0)
	{
		swapchainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapchainDetails.formats.data());
	}

	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	if (presentationCount > 0)
	{
		swapchainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapchainDetails.presentationModes.data());
	}

	return swapchainDetails;
}

VkSurfaceFormatKHR VkRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (auto& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
			&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return formats.front();
}

VkPresentModeKHR VkRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(_window, &width, &height);

		VkExtent2D extent = {};
		extent.width = static_cast<uint32_t>(width);
		extent.height = static_cast<uint32_t>(height);

		extent.width = std::max(extent.width, std::min(surfaceCapabilities.maxImageExtent.width, extent.width));
		extent.height = std::max(extent.height, std::min(surfaceCapabilities.maxImageExtent.height, extent.height));

		return extent;
	}
}

VkFormat VkRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	for (auto& format : formats)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Couldnae find image format");
	return {};
}

VkImage VkRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMem)
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.usage = useFlags;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkImage image;
	if (vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("Couldnae creat image");
	}

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memReqs);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memReqs.memoryTypeBits, propFlags);

	if (vkAllocateMemory(mainDevice.logicalDevice, &memAllocInfo, nullptr, imageMem) != VK_SUCCESS)
	{
		throw std::runtime_error("Couldnae alloc memory");
	}

	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMem, 0);

	return image;
}

VkImageView VkRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &createInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image view");
	}
	return imageView;
}

VkShaderModule VkRenderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &createInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module");
	}

	return shaderModule;
}
