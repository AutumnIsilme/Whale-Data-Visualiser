#include "pch.h"

#include "Renderer.h"

#include "Log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

Renderer::Renderer()
{
	// Must do nothing. There is only one of these.
}

Renderer::~Renderer()
{}

int Renderer::Init(int width, int height)
{
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		Log::Critical("Failed to load GLAD!");
		return -1;
	}

	glViewport(0, 0, width, height);

	return 0;
}

void Renderer::SetClearColour(float red, float green, float blue, float alpha)
{
	glClearColor(red, green, blue, alpha);
}

void Renderer::Clear()
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}