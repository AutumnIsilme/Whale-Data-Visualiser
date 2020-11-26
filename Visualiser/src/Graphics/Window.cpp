#include "pch.h"
#include "Window.h"
#include "Common.h"
#include "Log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

int Window::Init(uint32_t width, uint32_t height)
{
	int init_result = glfwInit();
	if (init_result != GLFW_TRUE)
	{
		Log::Critical("Failed to initialise GLFW!");
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	glfw_window = glfwCreateWindow(width, height, "Window", NULL, NULL);

	if (glfw_window == NULL)
	{
		Log::Critical("Failed to create GLFW window!");
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
