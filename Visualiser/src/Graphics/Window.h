#pragma once

#include "Common.h"

class Window
{
public:
	int Init(u32 width, u32 height);
	u32 Update();
	void SwapBuffers();
	void* GetPlatformWindow();

	Window(); // Constructor must do nothing.
	~Window();

private:
	struct GLFWwindow* glfw_window = NULL;
	bool init_succeeded = false;
};