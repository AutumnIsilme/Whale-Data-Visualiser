#include "Window.h"

#include "Common.h"
#include "Log.h"
#include "Application.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <bgfx/bgfx.h>

#define NULL 0

void window_size_callback(GLFWwindow* window, int width, int height)
{
	reset(width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (action)
		on_mouse_button_down_event(button);
	else
		on_mouse_button_up_event(button);
}

void cursor_pos_callback(GLFWwindow* window, double x, double y)
{
	on_mouse_move_event(x, y);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	/*if (key == GLFW_KEY_LEFT && action == GLFW_KEY_DOWN && mods == 0)
	{
		on_arrow_key_down_event(0);
	}
	if (key == GLFW_KEY_RIGHT && action == GLFW_KEY_DOWN && mods == 0)
	{
		on_arrow_key_down_event(1);
	}

	if (key == GLFW_KEY_LEFT && action == GLFW_KEY_UP && mods == 0)
	{
		on_arrow_key_up_event(0);
	}
	if (key == GLFW_KEY_RIGHT && action == GLFW_KEY_UP && mods == 0)
	{
		on_arrow_key_up_event(1);
	}*/
}

void window_close_callback(GLFWwindow* window)
{
}

int Window::Init(u32 width, u32 height, bool resizable)
{
	int init_result = glfwInit();
	if (init_result != GLFW_TRUE)
	{
		LOG_CRITICAL("Failed to initialise GLFW!");
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, resizable);

	glfw_window = glfwCreateWindow(width, height, "Visualiser", NULL, NULL);

	if (glfw_window == NULL)
	{
		LOG_CRITICAL("Failed to create GLFW window!");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(glfw_window);

	init_succeeded = true;

	glfwSetKeyCallback(glfw_window, key_callback);

	glfwSetMouseButtonCallback(glfw_window, mouse_button_callback);

	glfwSetCursorPosCallback(glfw_window, cursor_pos_callback);

	glfwSetScrollCallback(glfw_window, scroll_callback);

	glfwSetWindowSizeCallback(glfw_window, window_size_callback);

	glfwSetWindowCloseCallback(glfw_window, window_close_callback);

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

void* Window::GetGLFWWindow()
{
	return glfw_window;
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
