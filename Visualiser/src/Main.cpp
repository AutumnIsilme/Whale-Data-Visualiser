#include "Log.h"
#include "Graphics/Window.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

Window mainWindow;

#define WIDTH 1280
#define HEIGHT 720

/*
What do I want to be able to do?

-- Render model in framebuffer with specific rotation
-- Generate graph to texture
-- Display buttons and menu and things

*/

struct PosColorVertex
{
	float x;
	float y;
	float z;
	uint32_t abgr;
};

static PosColorVertex cubeVertices[] =
{
	{-1.0f,  1.0f,  1.0f, 0xff000000 },
	{ 1.0f,  1.0f,  1.0f, 0xff0000ff },
	{-1.0f, -1.0f,  1.0f, 0xff00ff00 },
	{ 1.0f, -1.0f,  1.0f, 0xff00ffff },
	{-1.0f,  1.0f, -1.0f, 0xffff0000 },
	{ 1.0f,  1.0f, -1.0f, 0xffff00ff },
	{-1.0f, -1.0f, -1.0f, 0xffffff00 },
	{ 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t cubeTriList[] =
{
	0, 1, 2,
	1, 3, 2,
	4, 6, 5,
	5, 6, 7,
	0, 2, 4,
	4, 2, 6,
	1, 5, 3,
	5, 7, 3,
	0, 4, 1,
	4, 5, 1,
	2, 3, 6,
	6, 3, 7,
};

bgfx::ShaderHandle loadDefaultShader(const char* filename)
{
	std::string filepath = "X:\\Data visualiser\\Visualiser\\vendor\\bgfx\\examples\\runtime\\shaders";
	switch (bgfx::getRendererType())
	{
	case bgfx::RendererType::Noop:
	case bgfx::RendererType::Direct3D9: filepath.append("\\dx9\\"); break;
	case bgfx::RendererType::Direct3D11:
	case bgfx::RendererType::Direct3D12: filepath.append("\\dx11\\"); break;
	case bgfx::RendererType::Gnm: filepath.append("\\pssl\\"); break;
	case bgfx::RendererType::Metal: filepath.append("\\metal\\"); break;
	case bgfx::RendererType::OpenGL: filepath.append("\\glsl\\"); break;
	case bgfx::RendererType::OpenGLES: filepath.append("\\essl\\"); break;
	case bgfx::RendererType::Vulkan: filepath.append("\\spirv\\"); break;
	default:
		break;
	}
	filepath.append(filename);

	LOG_INFO("Opening shader at: {}", filepath);
	FILE* file = fopen(filepath.c_str(), "rb");
	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	const bgfx::Memory* mem = bgfx::alloc(filesize + 1);
	fread(mem->data, 1, filesize, file);
	mem->data[mem->size - 1] = '\0';
	fclose(file);

	return bgfx::createShader(mem);
}

int main(int argc, char** argv)
{
	Log::Init();

	// @TODO: get monitor size or something to set resolution
	if (mainWindow.Init(WIDTH, HEIGHT) == -1)
	{
		LOG_CRITICAL("Window init failed, terminating.");
		return -1;
	}
	LOG_INFO("Initialised window!");

	bgfx::PlatformData platform_data;
	platform_data.nwh = mainWindow.GetPlatformWindow();
	bgfx::setPlatformData(platform_data);

	bgfx::Init init_params;
	init_params.type = bgfx::RendererType::Count;
	init_params.resolution.width = WIDTH;
	init_params.resolution.height = HEIGHT;
	init_params.resolution.reset = BGFX_RESET_VSYNC;
	bgfx::init(init_params);

	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x326fa8ff, 1.0f, 0);
	bgfx::setViewRect(0, 0, 0, WIDTH, HEIGHT);

	bgfx::VertexLayout layout;
	layout.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();
	bgfx::VertexBufferHandle vertex_buffer = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), layout);
	bgfx::IndexBufferHandle index_buffer = bgfx::createIndexBuffer(bgfx::makeRef(cubeTriList, sizeof(cubeTriList)));

	bgfx::ShaderHandle vertex_shader = loadDefaultShader("vs_cubes.bin");
	bgfx::ShaderHandle fragment_shader = loadDefaultShader("fs_cubes.bin");
	bgfx::ProgramHandle shader_program = bgfx::createProgram(vertex_shader, fragment_shader, true);

	u32 counter = 0;													 
	while (1)
	{
		glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), { 0.0f, 1.0f, 0.0f });
		glm::mat4 projection = glm::perspective(glm::radians(60.0f), float(WIDTH) / float(HEIGHT), 0.1f, 100.0f);
		bgfx::setViewTransform(0, &view[0][0], &projection[0][0]);
		glm::mat4 rotation = glm::identity<glm::mat4>();
		rotation *= glm::yawPitchRoll(counter * 0.01f, counter * 0.01f, 0.0f);
		bgfx::setTransform(&rotation[0][0]);

		bgfx::setVertexBuffer(0, vertex_buffer);
		bgfx::setIndexBuffer(index_buffer);
		bgfx::submit(0, shader_program);

		bgfx::frame();
		counter++;

		if (mainWindow.Update())
		{
			break;
		}
	}

	bgfx::destroy(vertex_buffer);
	bgfx::destroy(index_buffer);
	bgfx::shutdown();

	return 0;
}
