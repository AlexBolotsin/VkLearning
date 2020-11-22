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

	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;

		if (angle > 360.f) {
			angle -= 360.f;
		}
		vkRenderer.updateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f)));
		vkRenderer.Draw();
	}
	vkRenderer.Cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}