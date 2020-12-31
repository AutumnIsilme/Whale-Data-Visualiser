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
std::vector<float> depth_x_data;
u8 just_loaded_depth = 0;

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
	6, 3, 7,
	2, 3, 6,
	4, 5, 1,
	0, 4, 1,
	5, 7, 3,
	1, 5, 3,
	4, 2, 6,
	0, 2, 4,
	5, 6, 7,
	4, 6, 5,
	1, 3, 2,
	0, 1, 2,
};

void reset(s32 width, s32 height)
{
	WIDTH = width;
	HEIGHT = height;

	bgfx::reset(width, height, BGFX_RESET_VSYNC);
	imguiReset(width, height);

	bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00FF00FF, 1.0f, 0);
	bgfx::setViewRect(0, 0, 0, WIDTH, HEIGHT);
	/*bgfx::setViewFrameBuffer(1, window2fbh);
	bgfx::setViewClear(1, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0xFF00FFFF, 1.0f, 0);
	bgfx::setViewRect(1, 0, 0, 480, 360);*/
}

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

// @TODO: Move this to a worker thread so it doesn't block the main window
void LoadDepthData()
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
	}

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* mem = (char*)malloc(filesize+1);
	fread(mem, 1, filesize, file);
	mem[filesize] = '\n';
	fclose(file);

	// @TODO: Maybe hold onto the old data to not replace it if the input is bad?
	depth_data.clear();
	depth_x_data.clear();

	for (u32 index = 0; index < filesize; index++)
	{
		u32 start = index;
		while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
		{
			index++;
		}
		if (mem[index] != '\n')
		{
			LOG_ERROR("Malformed depth data input: Expected number or newline, got {0} at {1}.", mem[index], index);
			goto cleanup;
		}
		depth_data.push_back(-std::stof(std::string(&mem[start], index - start)));
		depth_x_data.push_back(depth_x_data.size());
	}
	LOG_INFO("Loaded depth data from \"{0}\".", filepath);
	just_loaded_depth = 3;
cleanup:
	free(mem);
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

	bgfx::ShaderHandle vertex_shader = loadDefaultShader("vs_cubes.bin");
	bgfx::ShaderHandle fragment_shader = loadDefaultShader("fs_cubes.bin");
	bgfx::ProgramHandle shader_program = bgfx::createProgram(vertex_shader, fragment_shader, true);*/

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

	reset(WIDTH, HEIGHT);

	u32 counter = 0;
	float time, lastTime = 0;
	float dt;
	bool running = true;
	bool dockspace_open = true;
	while (running)
	{
		bgfx::touch(0);
		bgfx::touch(1);
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

		//glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), { 0.0f, 1.0f, 0.0f });
		//glm::mat4 projection = glm::perspective(glm::radians(60.0f), float(WIDTH) / float(HEIGHT), 0.1f, 100.0f);
		//bgfx::setViewTransform(0, &view[0][0], &projection[0][0]);
		//glm::mat4 rotation = glm::mat4(1.0f);
		//rotation = glm::translate(rotation, { 1.0f, 0.0f, 0.0f });
		//rotation *= glm::yawPitchRoll(counter * 0.01f, counter * 0.01f, 0.0f);
		//bgfx::setTransform(&rotation[0][0]);

		//bgfx::setVertexBuffer(0, vertex_buffer);
		//bgfx::setIndexBuffer(index_buffer);
		//bgfx::submit(0, shader_program);

		//projection = glm::perspective(glm::radians(60.0f), float(480) / float(360), 0.1f, 100.0f);
		//bgfx::setViewTransform(1, &view[0][0], &projection[0][0]);

		//bgfx::setTransform(&rotation[0][0]);
		//bgfx::setVertexBuffer(0, vertex_buffer);
		//bgfx::setIndexBuffer(index_buffer);
		//bgfx::submit(1, shader_program);
		//ImGui::ShowDemoWindow();

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
		std::string layout_path = "";
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::BeginMenu("Open"))
				{
					if (ImGui::MenuItem("Depth Data"))
					{
						LoadDepthData();
					}
					if (ImGui::MenuItem("Orientation Data"))
					{

					}
					if (ImGui::MenuItem("Cached Data"))
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
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Begin("Orientation Graphs");
		ImGui::End();

		ImGui::Begin("3D Viewport");
		ImGui::End();

		ImGui::Begin("Depth Graph");
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
			ImPlot::PlotLine("Data", &depth_x_data.data()[start], &depth_data.data()[start], size, 0, sizeof(float) * downsample);
			ImPlot::EndPlot();
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

		if (just_loaded_depth)
			just_loaded_depth -= 1;
	}
	LOG_INFO("Shutting down.");

	imguiShutdown();
	/*bgfx::destroy(vertex_buffer);
	bgfx::destroy(index_buffer);*/
	bgfx::shutdown();


	return 0;
}
