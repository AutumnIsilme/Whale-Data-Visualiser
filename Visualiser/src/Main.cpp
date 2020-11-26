#include "pch.h"
#include "Graphics/Window.h"
#include "Graphics/Renderer.h"
#include "Log.h"

Window mainWindow;

// At most 16 windows may be present.
// OpenGL context may be shared with the main window?
Window windows[15];

Renderer renderer;

#define WIDTH 1280
#define HEIGHT 720

/*
What do I want to be able to do?

-- Render model in framebuffer with specific rotation
-- Generate graph to texture
-- Display buttons and menu and things

*/

int main(int argc, char** argv)
{
	// @TODO: get monitor size or something to set resolution
	if (mainWindow.Init(WIDTH, HEIGHT) == -1)
	{
		Log::Critical("Window init failed, terminating.");
		return -1;
	}
	Log::Info("Initialised window!");

	if (renderer.Init(WIDTH, HEIGHT) == -1)
	{
		Log::Critical("Renderer init failed, terminating.");
		return -1;
	}
	Log::Info("Initialised renderer!");

	renderer.SetClearColour(1.0f, 1.0f, 1.0f, 1.0f);
	
	while (1)
	{
		// render_model(next_rotation);

		mainWindow.SwapBuffers();

		renderer.Clear();

		if (mainWindow.Update())
		{
			break;
		}
	}

	return 0;
}
