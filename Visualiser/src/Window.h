#pragma once

#include "Common.h"

class Window
{
public:
	int Init(u32 *width, u32 *height, bool resizable);
	u32 Update();
	void SwapBuffers();
	void* GetPlatformWindow();
	void* GetGLFWWindow();

	Window(); // Constructor must do nothing.
	~Window();

private:
	struct GLFWwindow* glfw_window = NULL;
	bool init_succeeded = false;
};

void cursor_pos_callback(struct GLFWwindow* window, double x, double y);
