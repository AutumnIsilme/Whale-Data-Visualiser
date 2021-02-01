#include "Log.h"
#include "Window.h"
#include "Application.h"

#include "Shaders.h"
#include "TextureShaders.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <GLFW/glfw3.h>

#include <nfd.h>

#include <imgui.h>
#include "ImGuiImpl.h"

#include <implot.h>

#include <stb_image.h>

#include <string>
#include <vector>
#include <cstdlib>

_getShader(vs_texture);
_getShader(fs_texture);

Window mainWindow;

u32 WIDTH = 1280;
u32 HEIGHT = 720;

/*
What do I want to be able to do?

-- Render model in framebuffer with specific rotation
-- Generate graph to texture
-- Display buttons and menu and things

*/

std::vector<float> depth_data;
std::vector<float> pitch_data;
std::vector<float> roll_data;
std::vector<float> yaw_data;
std::vector<float> yaw_sin_data;
std::vector<float> x_data;
u8 just_loaded_depth = 0;
u8 just_loaded_pitch = 0;
u8 just_loaded_roll  = 0;
u8 just_loaded_yaw   = 0;

struct PosColorVertex
{
	float x;
	float y;
	float z;
	uint32_t abgr;
};

struct PosColTexVertex
{
	float x;
	float y;
	float z;
	uint32_t abgr;
	float u;
	float v;
};

static PosColTexVertex pointerVertices[] =
{
	{ 0.0f,  2.0f,  0.0f, 0xff0000ff, 0.0f, 0.0f },
	{ 0.0f, -1.0f,  1.0f, 0xff00ff00, 0.0f, 0.0f },
	{ 0.5f, -1.0f, -1.0f, 0xff00ffff, 0.0f, 0.0f },
	{-0.5f, -1.0f, -1.0f, 0xffff0000, 0.0f, 0.0f },
};

static const uint16_t pointerTriList[] =
{
	0, 1, 2,
	0, 3, 1,
	0, 2, 3,
	1, 3, 2,
};

static PosColorVertex s_cubeVertices[] =
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

static PosColTexVertex s_XZAxes[] =
{
	{-3.0f, -3.0f, -3.0f,   0xffffffff,   0.0f, 0.0f},
	{-3.0f,  3.0f, -3.0f,   0xffffffff,   0.0f, 1.0f},
	{ 3.0f,  3.0f, -3.0f,   0xffffffff,   1.0f, 1.0f},
	{ 3.0f, -3.0f, -3.0f,   0xffffffff,   1.0f, 0.0f}
};

static PosColTexVertex s_XYAxes[] =
{
	{-3.0f,  3.0f, -3.0f,   0xffffffff,   0.0f, 0.0f},
	{-3.0f,  3.0f,  3.0f,   0xffffffff,   0.0f, 1.0f},
	{ 3.0f,  3.0f,  3.0f,   0xffffffff,   1.0f, 1.0f},
	{ 3.0f,  3.0f, -3.0f,   0xffffffff,   1.0f, 0.0f}
};

static PosColTexVertex s_YZAxes[] =
{
	{-3.0f, -3.0f, -3.0f,   0xffffffff,   0.0f, 0.0f},
	{-3.0f, -3.0f,  3.0f,   0xffffffff,   0.0f, 1.0f},
	{-3.0f,  3.0f,  3.0f,   0xffffffff,   1.0f, 1.0f},
	{-3.0f,  3.0f, -3.0f,   0xffffffff,   1.0f, 0.0f}
};

/*
static PosColorVertex s_XZAxes[] =
{
	{-3.0f, -3.0f, -3.0f,   0xff000000},
	{-3.0f,  3.0f, -3.0f,   0xff0000ff},
	{ 3.0f,  3.0f, -3.0f,   0xff00ffff},
	{ 3.0f, -3.0f, -3.0f,   0xffffffff}
};

static PosColorVertex s_XYAxes[] =
{
	{-3.0f,  3.0f, -3.0f,   0xff000000},
	{-3.0f,  3.0f,  3.0f,   0xff0000ff},
	{ 3.0f,  3.0f,  3.0f,   0xff00ffff},
	{ 3.0f,  3.0f, -3.0f,   0xffffffff}
};

static PosColorVertex s_YZAxes[] =
{
	{-3.0f, -3.0f, -3.0f,   0xff000000},
	{-3.0f, -3.0f,  3.0f,   0xff0000ff},
	{-3.0f,  3.0f,  3.0f,   0xff00ffff},
	{-3.0f,  3.0f, -3.0f,   0xffffffff}
};*/

static const uint16_t s_AxesTris[] =
{
	0, 1, 2,
	0, 2, 3,
	0, 2, 1,
	0, 3, 2
};

static const uint16_t s_cubeTriList[] =
{
	0, 2, 1, // 0
	1, 2, 3,
	4, 5, 6, // 2
	5, 7, 6,
	0, 4, 2, // 4
	4, 6, 2,
	1, 3, 5, // 6
	5, 3, 7,
	0, 1, 4, // 8
	4, 1, 5,
	2, 6, 3, // 10
	6, 7, 3,
};

bgfx::TextureHandle play_button_texture_play;
bgfx::TextureHandle play_button_texture_pause;
bgfx::TextureHandle settings_button_texture;
bgfx::TextureHandle xy_axes_texture;
bgfx::TextureHandle xz_axes_texture;
bgfx::TextureHandle yz_axes_texture;
bgfx::TextureHandle whale_texture;

void reset(s32 width, s32 height)
{
	WIDTH = width;
	HEIGHT = height;

	bgfx::reset(width, height, BGFX_RESET_VSYNC);
	imguiReset(width, height);

	bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000FF, 1.0f, 0);
	bgfx::setViewRect(0, 0, 0, WIDTH, HEIGHT);
}

bgfx::ShaderHandle loadDefaultShader(const char* filename)
{
	std::string filepath = "assets\\shaders\\";
	switch (bgfx::getRendererType())
	{
	case bgfx::RendererType::Noop:
	case bgfx::RendererType::Direct3D9: filepath.append("dx9\\"); break;
	case bgfx::RendererType::Direct3D11:
	case bgfx::RendererType::Direct3D12: filepath.append("dx11\\"); break;
	case bgfx::RendererType::Gnm: filepath.append("pssl\\"); break;
	case bgfx::RendererType::Metal: filepath.append("metal\\"); break;
	case bgfx::RendererType::OpenGL: filepath.append("glsl\\"); break;
	case bgfx::RendererType::OpenGLES: filepath.append("essl\\"); break;
	case bgfx::RendererType::Vulkan: filepath.append("spirv\\"); break;
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

enum class DataType
{
	DEPTH = 0,
	PITCH,
	ROLL,
	YAW,

	COUNT
};

// @TODO: Move this to a worker thread so it doesn't block the main window
void LoadData(std::vector<float>& data, DataType type)
{
	char* filepath = NULL;
	nfdresult_t open_result = NFD_OpenDialog(NULL, NULL, &filepath);
	if (open_result == NFD_CANCEL)
	{
		return;
	}
	else if (open_result == NFD_ERROR)
	{
		LOG_ERROR("Open dialog failed: {}", NFD_GetError());
		return;
	}
	
	FILE* file = fopen(filepath, "rb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		return;
	}

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* mem = (char*)malloc(filesize+1);
	fread(mem, 1, filesize, file);
	mem[filesize] = '\n';
	fclose(file);

	// @TODO: Maybe hold onto the old data to not replace it if the input is bad?
	data.clear();
	bool do_set_x_data = false;
	if (x_data.size() == 0)
		do_set_x_data = true;
	//depth_x_data.clear();

	for (u32 index = 0; index < filesize; index++)
	{
		u32 start = index;
		while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
		{
			index++;
		}
		if (mem[index] != '\n')
		{
			LOG_ERROR("Malformed data input: Expected number or newline, got {0} at {1}.", mem[index], index);
			goto cleanup;
		}
		data.push_back(-std::stof(std::string(&mem[start], index - start)));
		if (do_set_x_data)
			x_data.push_back(x_data.size());
		if (type == DataType::YAW)
			yaw_sin_data.push_back(glm::sin(-std::stof(std::string(&mem[start], index - start))));
	}

	switch (type)
	{
	case DataType::DEPTH:
		LOG_INFO("Loaded depth data from \"{0}\".", filepath);
		just_loaded_depth = 3;
		break;
	case DataType::PITCH:
		LOG_INFO("Loaded pitch data from \"{0}\".", filepath);
		just_loaded_pitch = 3;
		break;
	case DataType::ROLL:
		LOG_INFO("Loaded roll data from \"{0}\".", filepath);
		just_loaded_roll = 3;
		break;
	case DataType::YAW:
		LOG_INFO("Loaded heading data from \"{0}\".", filepath);
		just_loaded_yaw = 3;
		break;

	case DataType::COUNT:
	default:
		LOG_WARN("Unrecognised data type provided, loaded from \"{0}\".", filepath);
		break;
	}
cleanup:
	free(mem);
}

void LoadPlayPauseTextures()
{
	int width, height, channels;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc *data = stbi_load("assets/play.png", &width, &height, &channels, 0);
	play_button_texture_play = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/pause.png", &width, &height, &channels, 0);
	play_button_texture_pause = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/settings.png", &width, &height, &channels, 0);
	settings_button_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/XY_Axes.png", &width, &height, &channels, 0);
	xy_axes_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/XZ_Axes.png", &width, &height, &channels, 0);
	xz_axes_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/YZ_Axes.png", &width, &height, &channels, 0);
	yz_axes_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/white.png", &width, &height, &channels, 0);
	whale_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));
}

bool create_menu_bar(bool running, bool *need_load_layout, bool *need_save_layout, bool *use_default_layout)
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::BeginMenu("Open"))
			{
				if (ImGui::MenuItem("Depth Data"))
				{
					LoadData(depth_data, DataType::DEPTH);
				}
				if (ImGui::MenuItem("Pitch Data"))
				{
					LoadData(pitch_data, DataType::PITCH);
				}
				if (ImGui::MenuItem("Roll Data"))
				{
					LoadData(roll_data, DataType::ROLL);
				}
				if (ImGui::MenuItem("Heading Data"))
				{
					LoadData(yaw_data, DataType::YAW);
				}
				if (ImGui::MenuItem("Depth Cache"))
				{

				}
				if (ImGui::MenuItem("Orientation Cache"))
				{

				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Save"))
			{
				if (ImGui::MenuItem("Depth Cache"))
				{

				}
				if (ImGui::MenuItem("Orientation Cache"))
				{

				}
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Exit")) running = false;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			if (ImGui::MenuItem("Default Layout"))
			{
				*need_load_layout = true;
				*use_default_layout = true;
			}
			if (ImGui::MenuItem("Save Default Layout"))
			{
				*need_save_layout = true;
				*use_default_layout = true;
			}
			if (ImGui::MenuItem("Load Layout"))
			{
				*need_load_layout = true;
			}
			if (ImGui::MenuItem("Save Layout"))
			{
				*need_save_layout = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
	return running;
}

static double cursor_x;
static double cursor_y;

static double frame_x;
static double frame_y;
static double frame_width;
static double frame_height;

static bool changing_orientation = false;
static double camera_heading = 0.0;
static double camera_pitch = 0.0;

static ImGuiID viewport_viewport;

void on_mouse_button_down_event(int button)
{
	auto io = ImGui::GetIO();
	if (button != 0)
	{
		return;
	}
	if (frame_x <= cursor_x && frame_x + frame_width >= cursor_x &&
		frame_y <= cursor_y && frame_y + frame_height >= cursor_y &&
		io.MouseHoveredViewport == viewport_viewport)
	{
		changing_orientation = true;
	}
}

void on_mouse_button_up_event(int button)
{
	if (button == 0)
		changing_orientation = false;
}

void on_mouse_move_event(double x, double y)
{
	if (changing_orientation)
	{
		double dx = x - cursor_x;
		double dy = y - cursor_y;
		camera_heading += dx / frame_width * 6.283185307179586;
		camera_pitch += dy / frame_height * 3.141592653589793;
		if (camera_pitch > 1.5707963267948966)
			camera_pitch = 1.5707963267948966;
		else if (camera_pitch < -1.5707963267948966)
			camera_pitch = -1.5707963267948966;
		if (camera_heading > 3.141592653589793)
			camera_heading -= 6.283185307179586;
		else if (camera_heading < -3.141592653589793)
			camera_heading += 6.283185307179586;
	}

	cursor_x = x;
	cursor_y = y;
}

int main(int argc, char** argv)
{
	Log::Init();

	// @TODO: get monitor size or something to set resolution
	if (mainWindow.Init(WIDTH, HEIGHT, true) == -1)
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

	LOG_INFO("Renderer: {0} via bgfx", bgfx::getRendererName(bgfx::getRendererType()));

	/*bgfx::VertexLayout layout;
	layout.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();
	bgfx::VertexBufferHandle vertex_buffer = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), layout);
	bgfx::IndexBufferHandle index_buffer = bgfx::createIndexBuffer(bgfx::makeRef(cubeTriList, sizeof(cubeTriList)));
	*/
	bgfx::ShaderHandle vertex_shader = loadDefaultShader("vs_cubes.bin");
	bgfx::ShaderHandle fragment_shader = loadDefaultShader("fs_cubes.bin");
	bgfx::ProgramHandle shader_program = bgfx::createProgram(vertex_shader, fragment_shader, true);

	bgfx::ShaderHandle axes_vertex_shader = loadDefaultShader("vs_update.bin");
	bgfx::ShaderHandle axes_fragment_shader = loadDefaultShader("fs_update.bin");
	bgfx::ProgramHandle axes_shader_program = bgfx::createProgram(axes_vertex_shader, axes_fragment_shader, true);

	/*float* vertices = NULL;
	int* indices = NULL;
	int count = 0;
	LoadStlModel("assets/basic_model.stl", (char**)&vertices, &indices, &count);
	*/
	bgfx::VertexLayout layout;
	layout.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();

	bgfx::VertexLayout pos_tex_layout;
	pos_tex_layout.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();
	bgfx::UniformHandle axes_uniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
	bgfx::ProgramHandle axes_shader;
	bgfx::ShaderHandle vs = bgfx::createShader(bgfx::makeRef(vs_texture(), vs_texture_len()));
	bgfx::ShaderHandle fs = bgfx::createShader(bgfx::makeRef(fs_texture(), fs_texture_len()));
	axes_shader = bgfx::createProgram(vs, fs, true);
	bgfx::VertexBufferHandle XZ_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_XZAxes, sizeof(s_XZAxes)), pos_tex_layout);
	bgfx::VertexBufferHandle XY_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_XYAxes, sizeof(s_XYAxes)), pos_tex_layout);
	bgfx::VertexBufferHandle YZ_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_YZAxes, sizeof(s_YZAxes)), pos_tex_layout);
	bgfx::IndexBufferHandle axes_ibuffer = bgfx::createIndexBuffer(bgfx::makeRef(s_AxesTris, sizeof(s_AxesTris)));

	bgfx::VertexBufferHandle vertex_buffer = bgfx::createVertexBuffer(bgfx::makeRef(pointerVertices, sizeof(pointerVertices)), pos_tex_layout);
	bgfx::IndexBufferHandle index_buffer = bgfx::createIndexBuffer(bgfx::makeRef(pointerTriList, sizeof(pointerTriList)));

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	ImPlot::CreateContext();

	imguiSetUserMousePosCallback((void(*)(void*, double, double))cursor_pos_callback);

	imguiInit(&mainWindow);
	imguiReset(WIDTH, HEIGHT);

	imguiInstallCallbacks();

	/*Window window2;
	window2.Init(480, 360, false);
	window2fbh = bgfx::createFrameBuffer(window2.GetPlatformWindow(), 480, 360);
	bgfx::setViewClear(1, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0xFF00FFFF, 1.0f, 0);
	bgfx::setViewRect(1, 0, 0, 480, 360);*/

	bgfx::FrameBufferHandle model_viewport = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle texture_handle = BGFX_INVALID_HANDLE;
	glm::vec2 model_viewport_size_cache = glm::vec2(0.0f, 0.0f);

	reset(WIDTH, HEIGHT);

	LoadPlayPauseTextures();

	u32 counter = 0;
	float time, lastTime = 0;
	float dt;
	float sum_dt = 0.0f;
	bool running = true;
	bool playing = false;
	bool dockspace_open = true;
	bool do_overwrite_default_settings_dialog = false;
	bool do_overwrite_default_settings = false;
	float temporal_index = 0;
	float flow_rate = 1.0f;
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), { 0.0f, 0.0f, 1.0f });
	glm::mat4 projection = glm::mat4(1.0f);
	glm::vec3 orientation = glm::vec3(0.0f);

	while (running)
	{
		bgfx::touch(0);
		time = (float)glfwGetTime();
		dt = time - lastTime;
		lastTime = time;
		if (mainWindow.Update())
		{
			running = false;
			break;
		}
		if (WIDTH == 0 || HEIGHT == 0)
			continue;
		imguiEvents(dt);
		ImGui::NewFrame();

		/*
		bgfx::setTransform(&rotation[0][0]);
		bgfx::setVertexBuffer(0, vertex_buffer);
		bgfx::setIndexBuffer(index_buffer);
		bgfx::submit(1, shader_program);
		ImGui::ShowDemoWindow();
		*/

		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace", &dockspace_open, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

		bool need_load_layout = false;
		bool need_save_layout = false;
		bool use_default_layout = false;
		running = create_menu_bar(running, &need_load_layout, &need_save_layout, &use_default_layout);

		ImGui::Begin("Timeline");
		{
			if (ImGui::ImageButton(IMGUI_TEXTURE_FROM_BGFX(playing ? play_button_texture_play : play_button_texture_pause), ImVec2(13.0f, 13.0f)))
			{
				playing = !playing;
			}
			if (playing)
			{
				sum_dt += dt;
				while (sum_dt > (1.0f / 20.0f) / flow_rate)
				{
					temporal_index++;
					sum_dt -= (1.0f / 20.0f) / flow_rate;
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28);
			ImGui::SliderFloat("", &temporal_index, 0, x_data.size() - 1, "%.0f");
			ImGui::SameLine();

			if (ImGui::ImageButton(IMGUI_TEXTURE_FROM_BGFX(settings_button_texture), ImVec2(13.0f, 13.0f)))
			{
				ImGui::OpenPopup("Player Settings");
			}

			ImGui::SetNextWindowPos(ImVec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y - 64), ImGuiCond_Appearing);
			if (ImGui::BeginPopup("Player Settings"))
			{
				ImGui::SliderFloat("Rate", &flow_rate, 0.25f, 10.0f);
				if (ImGui::Button("Reset"))
					flow_rate = 1.0f;
				ImGui::EndPopup();
			}
		}
		ImGui::End();

		ImGui::Begin("Pitch Graph");
		{
			ImPlot::SetNextPlotLimits(-200, pitch_data.size() + 200, -3, 3, just_loaded_pitch ? ImGuiCond_Always : ImGuiCond_Once);
			if (ImPlot::BeginPlot("Pitch Data", "Time", "Pitch"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > pitch_data.size() - 1 ? pitch_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > pitch_data.size() - 1 ? pitch_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > pitch_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &pitch_data.data()[start], size, 0, sizeof(float) * downsample);
				if (pitch_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &pitch_data[(int)temporal_index], 1);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Roll Graph");
		{
			ImPlot::SetNextPlotLimits(-200, roll_data.size() + 200, -3, 3, just_loaded_roll ? ImGuiCond_Always : ImGuiCond_Once);
			if (ImPlot::BeginPlot("Roll Data", "Time", "Roll"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > roll_data.size() - 1 ? roll_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > roll_data.size() - 1 ? roll_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > roll_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &roll_data.data()[start], size, 0, sizeof(float) * downsample);
				if (roll_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &roll_data[(int)temporal_index], 1);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Yaw Graph");
		{
			ImPlot::SetNextPlotLimits(-200, yaw_data.size() + 200, -3, 3, just_loaded_yaw ? ImGuiCond_Always : ImGuiCond_Once);
			if (ImPlot::BeginPlot("Heading Data", "Time", "Yaw"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > yaw_data.size() - 1 ? yaw_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > yaw_data.size() - 1 ? yaw_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > yaw_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &yaw_data.data()[start], size, 0, sizeof(float) * downsample);
				if (yaw_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &yaw_data[(int)temporal_index], 1);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("3D Viewport");
		{
			viewport_viewport = ImGui::GetWindowViewport()->ID;
			ImVec2 available_space = ImGui::GetContentRegionAvail();
			frame_width = available_space.x;
			frame_height = available_space.y;
			ImVec2 loc = ImGui::GetCursorScreenPos();
			frame_x = loc.x - ImGui::GetWindowViewport()->Pos.x;
			frame_y = loc.y - ImGui::GetWindowViewport()->Pos.y;
			if (!bgfx::isValid(model_viewport) || available_space.x != model_viewport_size_cache.x || available_space.y != model_viewport_size_cache.y)
			{
				model_viewport_size_cache.x = available_space.x;
				model_viewport_size_cache.y = available_space.y;
				LOG_INFO("Recreating framebuffer: {0}, {1}.", model_viewport_size_cache.x, model_viewport_size_cache.y);
				if (bgfx::isValid(model_viewport))
				{
					bgfx::destroy(model_viewport);
				}
				texture_handle = bgfx::createTexture2D(available_space.x, available_space.y, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_RT);
				model_viewport = bgfx::createFrameBuffer(1, &texture_handle);
				bgfx::setViewFrameBuffer(1, model_viewport);
				bgfx::setViewClear(1, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x3483ebFF);
				bgfx::setViewRect(1, 0, 0, available_space.x, available_space.y);

				projection = glm::perspective(glm::radians(60.0f), float(available_space.x) / float(available_space.y), 0.1f, 100.0f);
				bgfx::setViewTransform(1, &view[0][0], &projection[0][0]);
			}
			glm::mat4 rotation = glm::mat4(1.0);
			glm::mat4 axes_rotation = glm::mat4(1.0);
			rotation *= glm::yawPitchRoll(0.0f, (float)camera_pitch, 0.0f);
			rotation *= glm::yawPitchRoll(0.0f, 0.0f, (float)camera_heading);
			axes_rotation *= glm::yawPitchRoll(0.0f, (float)camera_pitch, 0.0f);
			axes_rotation *= glm::yawPitchRoll(0.0f, 0.0f, (float)camera_heading);
			if (yaw_data.size() != 0 && pitch_data.size() != 0 && roll_data.size() != 0)
				rotation *= glm::yawPitchRoll(roll_data[temporal_index], pitch_data[temporal_index], yaw_data[temporal_index]);
			
			bool XZ_fore = camera_pitch < 0;// && !(camera_heading < -1.5707963267948966 || camera_heading > 1.5707963267948966) || camera_pitch > 0 && (camera_heading < -1.5707963267948966 || camera_heading > 1.5707963267948966);
			bool YZ_fore = camera_heading > 0;
			bool XY_fore = (camera_heading < -1.63/*5707963267948966*/ || camera_heading > 1.5707963267948966)  && !(camera_pitch < -1.4459012060099814 || camera_pitch > 1.4459012060099814);

			//uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_MSAA | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA) | 0x100005000000000;//0x10000500656501f; // Magic number that might work
			auto render_axis = [&](bgfx::VertexBufferHandle vbh, bgfx::TextureHandle th)
			{
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
				bgfx::setTransform(&axes_rotation[0][0]);
				bgfx::setVertexBuffer(1, vbh);
				bgfx::setIndexBuffer(axes_ibuffer);
				bgfx::setTexture(0, axes_uniform, th);
				bgfx::submit(1, axes_shader);
			};

			if (!XZ_fore)
				render_axis(XZ_vbuffer, xz_axes_texture);

			if (!XY_fore)
				render_axis(XY_vbuffer, xy_axes_texture);

			if (!YZ_fore)
				render_axis(YZ_vbuffer, yz_axes_texture);

			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
			bgfx::setTransform(&rotation[0][0]);
			bgfx::setVertexBuffer(1, vertex_buffer);
			bgfx::setIndexBuffer(index_buffer);
			bgfx::setTexture(0, axes_uniform, whale_texture);
			bgfx::submit(1, axes_shader);

			if (YZ_fore)
				render_axis(YZ_vbuffer, yz_axes_texture);

			if (XY_fore)
				render_axis(XY_vbuffer, xy_axes_texture);

			if (XZ_fore)
				render_axis(XZ_vbuffer, xz_axes_texture);

			bgfx::touch(1);

			ImGui::Image(IMGUI_TEXTURE_FROM_BGFX(texture_handle), available_space);
		}
		ImGui::End();

		ImGui::Begin("Depth Graph");
		{
			ImPlot::SetNextPlotLimits(-200, depth_data.size() + 200, -1500, 200, just_loaded_depth ? ImGuiCond_Always : ImGuiCond_Once);
			if (ImPlot::BeginPlot("Depth Data", "Time", "Depth"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > depth_data.size() - 1 ? depth_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > depth_data.size() - 1 ? depth_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > depth_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &depth_data.data()[start], size, 0, sizeof(float) * downsample);
				if (depth_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &depth_data[(int)temporal_index], 1);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::End();

		if (do_overwrite_default_settings_dialog)
		{
			ImGui::Begin("Overwrite default layout?", &do_overwrite_default_settings_dialog);
			{
				ImGui::Text("Are you sure you want to overwrite the default layout?");
				ImGui::Text("This is NOT recommended!");
				if (ImGui::Button("Confirm"))
				{
					do_overwrite_default_settings = true;
					do_overwrite_default_settings_dialog = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					do_overwrite_default_settings_dialog = false;
				}
			}
			ImGui::End();
		}

		ImGui::Render();
		imguiRender(ImGui::GetDrawData(), 200);
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		bgfx::frame();
		counter++;

		std::string layout_path = "";
		if (use_default_layout)
		{
			layout_path = "assets/default_layout.ini";
		}
		else if (need_load_layout || need_save_layout)
		{
			char* filepath = NULL;
			nfdresult_t open_result;
			if (need_load_layout)
				open_result = NFD_OpenDialog(NULL, NULL, &filepath);
			else
				open_result = NFD_SaveDialog(NULL, NULL, &filepath);
			if (open_result == NFD_CANCEL)
			{
				need_load_layout = need_save_layout = false;
			}
			else if (open_result == NFD_ERROR)
			{
				need_load_layout = need_save_layout = false;
				LOG_ERROR("Open dialog failed: {}", NFD_GetError());
			}
			else
			{
				layout_path = std::string(filepath);
				if (need_save_layout && layout_path.substr(layout_path.size() - 26, 26) == std::string("\\assets\\default_layout.ini"))
				{
					do_overwrite_default_settings_dialog = true;
				}
			}
		}
		if (need_load_layout)
		{
			// We are not allowed to load these while a frame is in progress, so we wait until afer ImGui::Render
			ImGui::LoadIniSettingsFromDisk(layout_path.c_str());
		}
		if (need_save_layout && !do_overwrite_default_settings_dialog)
		{
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
		}
		if (do_overwrite_default_settings)
		{
			layout_path = "assets/default_layout.ini";
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
			do_overwrite_default_settings = false;
		}

		if (just_loaded_depth)
			just_loaded_depth--;
		if (just_loaded_pitch)
			just_loaded_pitch--;
		if (just_loaded_roll)
			just_loaded_roll--;
		if (just_loaded_yaw)
			just_loaded_yaw--;
	}
	LOG_INFO("Shutting down.");

	imguiShutdown();
	/*bgfx::destroy(vertex_buffer);
	bgfx::destroy(index_buffer);*/
	bgfx::shutdown();

	return 0;
}
