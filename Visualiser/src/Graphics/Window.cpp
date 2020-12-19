#include "Graphics/Window.h"

#include "Common.h"
#include "Log.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define NULL 0

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	//glViewport(0, 0, width, height);
}

int Window::Init(u32 width, u32 height)
{
	int init_result = glfwInit();
	if (init_result != GLFW_TRUE)
	{
		LOG_CRITICAL("Failed to initialise GLFW!");
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	glfw_window = glfwCreateWindow(width, height, "Window", NULL, NULL);

	if (glfw_window == NULL)
	{
		LOG_CRITICAL("Failed to create GLFW window!");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(glfw_window);

	init_succeeded = true;
	return 0;
}

uint32_t Window::Update()
{
	glfwPollEvents();
	if (glfwWindowShouldClose(glfw_window))
	{
		return 1;
	}
	return 0;
}

void Window::SwapBuffers()
{
	glfwSwapBuffers(glfw_window);
}

void* Window::GetPlatformWindow()
{
	return glfwGetWin32Window(glfw_window);
}

Window::Window()
{
	// Constructor must do nothing.
}

Window::~Window()
{
	if (init_succeeded)
	{
		glfwDestroyWindow(glfw_window);
		glfwTerminate();
	}
}
