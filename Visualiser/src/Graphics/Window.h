#pragma once

#include "Common.h"

class Window
{
public:
	int Init(uint32_t width, uint32_t height);
	uint32_t Update();
	void SwapBuffers();

	Window(); // Constructor must do nothing.
	~Window();

private:
	struct GLFWwindow* glfw_window = NULL;
	bool init_succeeded = false;
};
