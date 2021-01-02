#include "Log.h"
#include "Window.h"
#include "Application.h"

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

static PosColorVertex pointerVertices[] =
{
	{ 0.0f,  2.0f,  0.0f, 0xff0000ff },
	{ 0.0f, -1.0f,  1.0f, 0xff00ff00 },
	{ 0.5f, -1.0f, -1.0f, 0xff00ffff },
	{-0.5f, -1.0f, -1.0f, 0xffff0000 },
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

void reset(s32 width, s32 height)
{
	WIDTH = width;
	HEIGHT = height;

	bgfx::reset(width, height, BGFX_RESET_VSYNC);
	imguiReset(width, height);

	bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00FF00FF, 1.0f, 0);
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
	bgfx::VertexBufferHandle vertex_buffer = bgfx::createVertexBuffer(bgfx::makeRef(pointerVertices, sizeof(pointerVertices)), layout);
	bgfx::IndexBufferHandle index_buffer = bgfx::createIndexBuffer(bgfx::makeRef(pointerTriList, sizeof(pointerTriList)));

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	ImPlot::CreateContext();

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
		std::string layout_path = "";
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
					need_load_layout = true;
					layout_path = "assets/default_layout.ini";
				}
				if (ImGui::MenuItem("Save Layout"))
				{
					need_save_layout = true;
					layout_path = "assets/default_layout.ini";
				}
				if (ImGui::MenuItem("Load Layout"))
				{

				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

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
				ImPlot::PlotLine("Data", &x_data.data()[start], &yaw_sin_data.data()[start], size, 0, sizeof(float) * downsample);
				if (yaw_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &yaw_data[(int)temporal_index], 1);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("3D Viewport");
		{
			ImVec2 available_space = ImGui::GetContentRegionAvail();
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
			glm::mat4 rotation = glm::mat4(1.0f);
			if (yaw_data.size() != 0 && pitch_data.size() != 0 && roll_data.size() != 0)
				rotation *= glm::yawPitchRoll(yaw_data[temporal_index], pitch_data[temporal_index], roll_data[temporal_index]);
			bgfx::setTransform(&rotation[0][0]);

			bgfx::setVertexBuffer(1, vertex_buffer);
			bgfx::setIndexBuffer(index_buffer);
			bgfx::submit(1, shader_program);
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

		ImGui::Render();
		imguiRender(ImGui::GetDrawData(), 200);
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		bgfx::frame();
		counter++;

		if (need_load_layout)
		{
			// We are not allowed to load these while a frame is in progress, so we wait until afer ImGui::Render
			ImGui::LoadIniSettingsFromDisk(layout_path.c_str());
		}
		if (need_save_layout)
		{
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
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
