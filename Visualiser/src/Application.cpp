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
#include <sstream>

_getShader(vs_texture);
_getShader(fs_texture);

Window mainWindow;

u32 WIDTH = 1280;
u32 HEIGHT = 720;

std::vector<float> depth_data;
std::vector<float> depth_velocity_data;
std::vector<float> depth_acceleration_data;
std::vector<float> pitch_data;
std::vector<float> roll_data;
std::vector<float> yaw_data;
std::vector<float> yaw_sin_data;
std::vector<float> x_data;
u8 just_loaded_depth = 0;
u8 just_loaded_pitch = 0;
u8 just_loaded_roll  = 0;
u8 just_loaded_yaw   = 0;

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

static std::string filepaths[4] = { "", "", "", "" };

/* Called when the window size changes and at startup to ensure clear colour is set and window size is known */
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

enum class DataType
{
	DEPTH = 0,
	PITCH,
	ROLL,
	YAW,

	COUNT
};

/* Load data from a file for a passed data type/buffer.
 * Called by LoadData and LoadDataCache. */
int ActuallyLoadData(std::vector<float>& data, DataType type, const char* filepath)
{
	FILE* file = fopen(filepath, "rb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		return 1;
	}

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Allocate space then read into that space the contents of the file
	char* mem = (char*)malloc(filesize + 1);
	defer { free(mem); };
	fread(mem, 1, filesize, file);
	mem[filesize] = '\n';
	fclose(file);

	// @TODO: Maybe hold onto the old data to not replace it if the input is bad?
	data.clear();
	if (type == DataType::DEPTH)
	{
		depth_velocity_data.clear();
		depth_acceleration_data.clear();
	}

	// @TODO: X data compatability check
	bool do_set_x_data = false;
	if (x_data.size() == 0)
		do_set_x_data = true;

	// Loop through the data in the file
	for (u32 index = 0; index < filesize; index++)
	{
		// At every iteration, we know we are at the start of a number
		u32 start = index;
		// Loop to the end of the number
		while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
		{
			index++;
		}
		// There's an unexpected character here, so stop
		if (mem[index] != '\n')
		{
			LOG_ERROR("Malformed data input: Expected number or newline, got {0} at {1}.", mem[index], index);
			return 2;
		}
		// Extract the number that we now know the start and length of, and turn it into a number
		data.push_back(-std::stof(std::string(&mem[start], index - start)));
		// If we haven't filled the x_data buffer yet (just a sequence of numbers up to the length of the data), add the next number
		if (do_set_x_data)
			x_data.push_back(x_data.size());
		// This was just testing what it would look like if I graphed the sin of the heading. It didn't work very well, but the code is still here.
		if (type == DataType::YAW)
			yaw_sin_data.push_back(glm::sin(-std::stof(std::string(&mem[start], index - start))));
	}

	// Set flags for which graph to adjust the zoom level of, and log success.
	switch (type)
	{
	case DataType::DEPTH:
	{
		static int average_width = 250;
		for (int i = 0; i < data.size(); i++)
		{
			if (i > average_width / 2 + (average_width % 2) + 1 && i < data.size() - average_width / 2 + (average_width % 2))
			{
				// Calculate velocity
				float sum = 0;
				for (int j = i - average_width / 2; j < i + average_width / 2 + (average_width % 2); j++)
				{
					sum += data[j] - data[j-1];
				}
				depth_velocity_data.push_back(sum / (float) average_width);
			} else
			{
				depth_velocity_data.push_back(0);
			}
		}
		for (int i = 0; i < depth_velocity_data.size(); i++)
		{
			if (i > average_width / 2 + (average_width % 2) + 1 && i < depth_velocity_data.size() - average_width / 2 + (average_width % 2))
			{
				// Calculate velocity
				float sum = 0;
				for (int j = i - average_width / 2; j < i + average_width / 2 + (average_width % 2); j++)
				{
					sum += depth_velocity_data[j] - depth_velocity_data[j - 1];
				}
				depth_acceleration_data.push_back(sum / (float)average_width);
			} else
			{
				depth_acceleration_data.push_back(0);
			}
		}
		/*if (depth_velocity_data.size() > 2)
			depth_velocity_data[0] = depth_velocity_data[1];*/
		LOG_INFO("Loaded depth data from \"{0}\".", filepath);
		just_loaded_depth = 3;
	} break;
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

	return 0;
}

/* Load data from a saved cache (currently just a list of filenames so you don't have to open each individually. */
void LoadDataCache()
{
	char* filepath = NULL;
	nfdresult_t open_result = NFD_OpenDialog(NULL, NULL, &filepath);
	defer{ free(filepath); };
	if (open_result == NFD_CANCEL)
	{
		return;
	} else if (open_result == NFD_ERROR)
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

	// Allocate space then read into that space the contents of the file
	char* mem = (char*)malloc(filesize + 1);
	defer{ free(mem); };
	fread(mem, 1, filesize, file);
	mem[filesize] = '\n';
	fclose(file);

	// Start the loop through the cache file
	char* head = mem;
	bool done[4] = { 0 };
	while (head < mem + filesize)
	{
		// Save the type of the data (indicated by a number at the start of an entry
		char type = *head;
		head += 2;
		u64 count = 0;
		// Go to the end of the line
		while (*head != '\n' && *head != '\r')
		{
			head++;
			count++;
		}
		// Extract the file name
		char* fname = (char*)malloc(count + 1);
		// Free the allocated memory when we exit the scope
		defer{ free(fname); };
		memcpy(fname, head - count, count);
		fname[count] = 0;

		switch (type)
		{
		case '0':
		{
			// If we have already filled out this data from this cache, complain, otherwise actually load the data
			if (!done[0])
			{
				int result = ActuallyLoadData(depth_data, DataType::DEPTH, fname);
				// If we succeeded in reading the data, save the filepath in case we want to cache it later
				if (!result)
					filepaths[0] = std::string(fname);
				done[0] = true;
			} else
			{
				LOG_WARN("Multiple data of the same type (Depth) present at filename {}", fname);
			}
		} break;
		case '1':
		{
			if (!done[1])
			{
				int result = ActuallyLoadData(pitch_data, DataType::PITCH, fname);
				if (!result)
					filepaths[1] = std::string(fname);
				done[1] = true;
			} else
			{
				LOG_WARN("Multiple data of the same type (Pitch) present at filename {}", fname);
			}
		} break;
		case '2':
		{
			if (!done[2])
			{
				int result = ActuallyLoadData(roll_data, DataType::ROLL, fname);
				if (!result)
					filepaths[2] = std::string(fname);
				done[2] = true;
			} else
			{
				LOG_WARN("Multiple data of the same type (Roll) present at filename {}", fname);
			}
		} break;
		case '3':
		{
			if (!done[3])
			{
				int result = ActuallyLoadData(yaw_data, DataType::YAW, fname);
				if (!result)
					filepaths[3] = std::string(fname);
				done[3] = true;
			} else
			{
				LOG_WARN("Multiple data of the same type (Heading) present at filename {}", fname);
			}
		} break;
		default:
			break;
		}

		// Go forward until we reach the next entry
		while (*head == '\n' || *head == '\r')
		{
			head++;
		}
	}
	LOG_INFO("Loaded data cache from {}", filepath);
}

/* Save a data cache to disk (Actually just a list of filenames to be loaded at one time later) */
void SaveDataCache()
{
	char* filepath = NULL;
	nfdresult_t open_result = NFD_SaveDialog(NULL, NULL, &filepath);
	if (open_result == NFD_CANCEL)
	{
		return;
	} else if (open_result == NFD_ERROR)
	{
		LOG_ERROR("Open dialog failed: {}", NFD_GetError());
		return;
	}

	// Open the file in write mode
	FILE* file = fopen(filepath, "wb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		return;
	}

	for (int i = 0; i < 4; i++)
	{
		if (filepaths[i].size() != 0)
		{
			// Write the data type and path to the file
			fprintf(file, "%d %s\n", i, filepaths[i].c_str());
		}
	}
	fclose(file);
	LOG_INFO("Saved data cache to {}", filepath);
	free(filepath);
}

// @TODO: Move this to a worker thread so it doesn't block the main window?
/* Load a specific data type from a file */
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
	int result = ActuallyLoadData(data, type, filepath);
	// If we succeeded, save the filepath in case we want to cache it later
	if (!result)
		filepaths[(int)type] = std::string(filepath);
	free(filepath);
}

/* Load in all the required textures */
void LoadTextures()
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

/* Create that menu bar at the top of the window */
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
				if (ImGui::MenuItem("Data Cache"))
				{
					LoadDataCache();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Save"))
			{
				if (ImGui::MenuItem("Data Cache"))
				{
					SaveDataCache();
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

/* When the mouse button is pressed, check if the cursor is inside the 3D viewport, and if so, set the flag to rotate the camera */
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

/* Stop rotating the camera if the mouse button is released */
void on_mouse_button_up_event(int button)
{
	if (button == 0)
		changing_orientation = false;
}

/* Move the camera when the mouse moves, if applicable */
void on_mouse_move_event(double x, double y)
{
	if (changing_orientation)
	{
		double dx = x - cursor_x;
		double dy = y - cursor_y;
		camera_heading += dx / frame_width * 6.283185307179586;
		camera_pitch += dy / frame_height * 3.141592653589793;
		// Keep the camera pitch in the correct range from straight up to straight down.
		if (camera_pitch > 1.5707963267948966)
			camera_pitch = 1.5707963267948966;
		else if (camera_pitch < -1.5707963267948966)
			camera_pitch = -1.5707963267948966;
		// Adjust the camera heading value so it remains in the range -pi to pi (Doesn't actually affect the rotation, but keeps the numbers accurate.
		if (camera_heading > 3.141592653589793)
			camera_heading -= 6.283185307179586;
		else if (camera_heading < -3.141592653589793)
			camera_heading += 6.283185307179586;
	}

	cursor_x = x;
	cursor_y = y;
}

/* Program entry point */
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

	// Tell the graphics library about the window and initialisation parameters
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

	// Set up the layout of the vertices.
	bgfx::VertexLayout pos_tex_layout;
	pos_tex_layout.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();

	// Initialise vertex and index buffers to render the objects
	bgfx::VertexBufferHandle XZ_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_XZAxes, sizeof(s_XZAxes)), pos_tex_layout);
	bgfx::VertexBufferHandle XY_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_XYAxes, sizeof(s_XYAxes)), pos_tex_layout);
	bgfx::VertexBufferHandle YZ_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_YZAxes, sizeof(s_YZAxes)), pos_tex_layout);
	bgfx::IndexBufferHandle axes_ibuffer = bgfx::createIndexBuffer(bgfx::makeRef(s_AxesTris, sizeof(s_AxesTris)));

	bgfx::VertexBufferHandle vertex_buffer = bgfx::createVertexBuffer(bgfx::makeRef(pointerVertices, sizeof(pointerVertices)), pos_tex_layout);
	bgfx::IndexBufferHandle index_buffer = bgfx::createIndexBuffer(bgfx::makeRef(pointerTriList, sizeof(pointerTriList)));

	// Initialise the shader program
	bgfx::UniformHandle texture_uniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
	bgfx::ShaderHandle vs = bgfx::createShader(bgfx::makeRef(vs_texture(), vs_texture_len()));
	bgfx::ShaderHandle fs = bgfx::createShader(bgfx::makeRef(fs_texture(), fs_texture_len()));
	bgfx::ProgramHandle shader_program = bgfx::createProgram(vs, fs, true);

	// Initialise the GUI library
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

	// Initialise variables used for the 3D viewport
	bgfx::FrameBufferHandle model_viewport = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle texture_handle = BGFX_INVALID_HANDLE;
	glm::vec2 model_viewport_size_cache = glm::vec2(0.0f, 0.0f);

	// Initialise clear colour and width/height
	reset(WIDTH, HEIGHT);

	LoadTextures();

	// Various variables used in the run loop
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
		// Clear the screen
		bgfx::touch(0);
		// Calculate the change in time since last frame
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
		// Tell the GUI library to process events and start a new frame
		imguiEvents(dt);
		ImGui::NewFrame();
		
		// Set up the main window to allow sub windows to dock into it
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

		// We can't load or save a layout in the middle of a frame, so these flags are kept separate and set to complete at the end of the frame
		bool need_load_layout = false;
		bool need_save_layout = false;
		bool use_default_layout = false;
		running = create_menu_bar(running, &need_load_layout, &need_save_layout, &use_default_layout);

		ImGui::Begin("Timeline");
		{
			// Play button
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

			if (temporal_index < 0)
				temporal_index = 0;
			else if (temporal_index >= x_data.size())
				temporal_index = x_data.size() - 1;

			ImGui::SameLine();

			// Settings button
			if (ImGui::ImageButton(IMGUI_TEXTURE_FROM_BGFX(settings_button_texture), ImVec2(13.0f, 13.0f)))
			{
				ImGui::OpenPopup("Player Settings");
			}

			// Show the popup window
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
				// Downsample technique borrowed from the ImPlot examples
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
				// Show current location in time
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
				// Downsample technique borrowed from the ImPlot examples
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
				// Show current location in time
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
				// Downsample technique borrowed from the ImPlot examples
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
				// Show current location in time
				if (yaw_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &yaw_data[(int)temporal_index], 1);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("3D Viewport");
		{
			auto io = ImGui::GetIO();

			// Go forward or backward in time if the arrow keys are pressed down
			static u64 left_down_prev = 0;
			static u64 right_down_prev = 0;
			static double left_next = .6;
			static double right_next = .6;
			if (io.KeysDown[GLFW_KEY_LEFT] && !left_down_prev)
			{
				temporal_index -= 20;
			}
			if (io.KeysDown[GLFW_KEY_RIGHT] && !right_down_prev)
			{
				temporal_index += 20;
			}
			if (!io.KeysDown[GLFW_KEY_LEFT] && left_down_prev)
			{
				left_next = .6;
			}
			if (!io.KeysDown[GLFW_KEY_RIGHT] && right_down_prev)
			{
				right_next = .6;
			}
			if (io.KeysDownDuration[GLFW_KEY_LEFT] > left_next)
			{
				left_next += 0.1;
				// Speed up the longer the key is pressed down
				if (left_next > 3)
					temporal_index -= 30;
				if (left_next > 8)
					temporal_index -= 50;
				if (left_next > 12)
					temporal_index -= 150;
				temporal_index -= 20;
			}
			if (io.KeysDownDuration[GLFW_KEY_RIGHT] > right_next)
			{
				right_next += 0.1;
				// Speed up the longer the key is pressed down
				if (right_next > 3)
					temporal_index += 30;
				if (left_next > 8)
					temporal_index += 50;
				if (left_next > 12)
					temporal_index += 150;
				temporal_index += 20;
			}

			left_down_prev = io.KeysDown[GLFW_KEY_LEFT];
			right_down_prev = io.KeysDown[GLFW_KEY_RIGHT];

			if (temporal_index < 0)
				temporal_index = 0;
			else if (temporal_index >= x_data.size())
				temporal_index = x_data.size() - 1;

			// Save the current viewport ID to detect if the user clicks in the viewport
			viewport_viewport = ImGui::GetWindowViewport()->ID;
			ImVec2 available_space = ImGui::GetContentRegionAvail();
			frame_width = available_space.x;
			frame_height = available_space.y;
			ImVec2 loc = ImGui::GetCursorScreenPos();
			frame_x = loc.x - ImGui::GetWindowViewport()->Pos.x;
			frame_y = loc.y - ImGui::GetWindowViewport()->Pos.y;
			// If the available space has changed or the model viewport doesn't exist, recreate it with the new size
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

				// Set camera projection with the new dimensions
				projection = glm::perspective(glm::radians(60.0f), float(available_space.x) / float(available_space.y), 0.1f, 100.0f);
				bgfx::setViewTransform(1, &view[0][0], &projection[0][0]);
			}
			// Rotate the whale pointer and the axes according to the current camera position
			glm::mat4 rotation = glm::mat4(1.0);
			glm::mat4 axes_rotation = glm::mat4(1.0);
			rotation *= glm::yawPitchRoll(0.0f, (float)camera_pitch, 0.0f);
			rotation *= glm::yawPitchRoll(0.0f, 0.0f, (float)camera_heading);
			axes_rotation *= glm::yawPitchRoll(0.0f, (float)camera_pitch, 0.0f);
			axes_rotation *= glm::yawPitchRoll(0.0f, 0.0f, (float)camera_heading);
			// If the data is loaded, rotate the whale pointer to the current orientation
			if (yaw_data.size() != 0 && pitch_data.size() != 0 && roll_data.size() != 0)
				rotation *= glm::yawPitchRoll(roll_data[temporal_index], pitch_data[temporal_index], yaw_data[temporal_index]);
			
			// Decide if each axis should be rendered in front of or behind the pointer because I couldn't figure out how to make that work by default
			bool XZ_fore = camera_pitch < 0;
			bool YZ_fore = camera_heading > 0;
			bool XY_fore = (camera_heading < -1.63 || camera_heading > 1.5707963267948966)  && !(camera_pitch < -1.4459012060099814 || camera_pitch > 1.4459012060099814);

			// Render axis function
			auto render_axis = [&](bgfx::VertexBufferHandle vbh, bgfx::TextureHandle th)
			{
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
				bgfx::setTransform(&axes_rotation[0][0]);
				bgfx::setVertexBuffer(1, vbh);
				bgfx::setIndexBuffer(axes_ibuffer);
				bgfx::setTexture(0, texture_uniform, th);
				bgfx::submit(1, shader_program);
			};

			// Render axes behind
			if (!XZ_fore)
				render_axis(XZ_vbuffer, xz_axes_texture);

			if (!XY_fore)
				render_axis(XY_vbuffer, xy_axes_texture);

			if (!YZ_fore)
				render_axis(YZ_vbuffer, yz_axes_texture);

			// Render pointer
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
			bgfx::setTransform(&rotation[0][0]);
			bgfx::setVertexBuffer(1, vertex_buffer);
			bgfx::setIndexBuffer(index_buffer);
			bgfx::setTexture(0, texture_uniform, whale_texture);
			bgfx::submit(1, shader_program);

			// Render axes in front
			if (YZ_fore)
				render_axis(YZ_vbuffer, yz_axes_texture);

			if (XY_fore)
				render_axis(XY_vbuffer, xy_axes_texture);

			if (XZ_fore)
				render_axis(XZ_vbuffer, xz_axes_texture);

			// Render the viewport to the screen
			ImGui::Image(IMGUI_TEXTURE_FROM_BGFX(texture_handle), available_space);
		}
		ImGui::End();

        ImGui::Begin("Depth Graph");
        {
            ImPlot::SetNextPlotLimits(-200, depth_data.size() + 200, -1500, 200, just_loaded_depth ? ImGuiCond_Always : ImGuiCond_Once);
            if (ImPlot::BeginPlot("Depth Data", "Time", "Depth"))
            {
                ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
                // Downsample technique borrowed from the ImPlot examples
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
                // Show current location in time
                if (depth_data.size() != 0)
                    ImPlot::PlotScatter("Current", &temporal_index, &depth_data[(int)temporal_index], 1);
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::Begin("Depth Velocity Graph");
        {
            ImPlot::SetNextPlotLimits(-200, depth_velocity_data.size() + 200, -0.2, 0.2, just_loaded_depth ? ImGuiCond_Always : ImGuiCond_Once);
            if (ImPlot::BeginPlot("Depth Velocity Data", "Time", "Velocity"))
            {
                ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
                // Downsample technique borrowed from the ImPlot examples
                int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
                int start = (int)ImPlot::GetPlotLimits().X.Min;
                start = start < 0 ? 0 : (start > depth_velocity_data.size() - 1 ? depth_velocity_data.size() - 1 : start);
                int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
                end = end < 0 ? 0 : end > depth_velocity_data.size() - 1 ? depth_velocity_data.size() - 1 : end;
                int size = (end - start) / downsample;
                while (size * downsample > depth_velocity_data.size())
                {
                    size--;
                }
                ImPlot::PlotLine("Data", &x_data.data()[start], &depth_velocity_data.data()[start], size, 0, sizeof(float) * downsample);
                // Show current location in time
                if (depth_velocity_data.size() != 0)
                    ImPlot::PlotScatter("Current", &temporal_index, &depth_velocity_data[(int)temporal_index], 1);
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::Begin("Depth Acceleration Graph");
        {
            ImPlot::SetNextPlotLimits(-200, depth_acceleration_data.size() + 200, -0.0004, 0.0004, just_loaded_depth ? ImGuiCond_Always : ImGuiCond_Once);
            if (ImPlot::BeginPlot("Depth Acceleration Data", "Time", "Acceleration"))
            {
                ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
                // Downsample technique borrowed from the ImPlot examples
                int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
                int start = (int)ImPlot::GetPlotLimits().X.Min;
                start = start < 0 ? 0 : (start > depth_acceleration_data.size() - 1 ? depth_acceleration_data.size() - 1 : start);
                int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
                end = end < 0 ? 0 : end > depth_acceleration_data.size() - 1 ? depth_acceleration_data.size() - 1 : end;
                int size = (end - start) / downsample;
                while (size * downsample > depth_acceleration_data.size())
                {
                    size--;
                }
                ImPlot::PlotLine("Data", &x_data.data()[start], &depth_acceleration_data.data()[start], size, 0, sizeof(float) * downsample);
                // Show current location in time
                if (depth_acceleration_data.size() != 0)
                    ImPlot::PlotScatter("Current", &temporal_index, &depth_acceleration_data[(int)temporal_index], 1);
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

		ImGui::End();

		// If the user selected to overwrite the default layout, ask if they are sure they want to do this
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

		// Finish the frame for both the GUI library and graphics library
		ImGui::Render();
		imguiRender(ImGui::GetDrawData(), 200);
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		bgfx::frame();
		counter++;

		// Check if the user asked to save or load layouts
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
		if (need_save_layout && !use_default_layout)
		{
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
		} else if (need_save_layout && use_default_layout)
		{
			do_overwrite_default_settings_dialog = true;
		}
		if (do_overwrite_default_settings)
		{
			layout_path = "assets/default_layout.ini";
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
			do_overwrite_default_settings = false;
		}

		// Decrement just loaded flags
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

	// Shut down the GUI library and graphics library before exiting.
	imguiShutdown();
	bgfx::shutdown();

	return 0;
}
