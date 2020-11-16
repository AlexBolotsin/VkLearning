#include <xmmintrin.h>

#include <iostream>

#define FLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <mat4x4.hpp>

#include <iostream>

#include "VkRenderer.h"

GLFWAPI GLFWwindow* window;

void initWindow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

#include <Windows.h>
int main()
{
	initWindow();

	VkRenderer vkRenderer;
	if (int error = vkRenderer.Init(window); error != 0)
	{
		return error;
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		vkRenderer.Draw();
	}
	vkRenderer.Cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}