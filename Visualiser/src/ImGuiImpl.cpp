/*
	This file is multiple files duct-taped together,
	some originally borrowed from https://github.com/Josh4428/bigg
	The code was originally released into the public domain

	Other bits are borrowed from the default ImGui backends for GLFW,
	released under the MIT license:

The MIT License (MIT)

Copyright (c) 2014-2020 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

	Retrieved 2020-12-19 to 2020-12-23
*/

#include "ImGuiImpl.h"

#include "Log.h"

#include "imgui_internal.h"

#include <bgfx/bgfx.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <d3d11.h>
#include <d3dcompiler.h>

#include <stdint.h>

#include "Shaders.h"
#include "ImGuiShaders.h"

_getShader(vs_ocornut_imgui);
_getShader(fs_ocornut_imgui);

static bgfx::VertexLayout  imguiVertexLayout;
static bgfx::TextureHandle imguiFontTexture;
static bgfx::UniformHandle imguiFontUniform;
static bgfx::ProgramHandle imguiProgram;

static GLFWwindow* g_Window = NULL;
static GLFWcursor* g_MouseCursors[ImGuiMouseCursor_COUNT] = { 0 };
static bool                 g_MouseJustPressed[ImGuiMouseButton_COUNT] = {};
static bool                 g_InstalledCallbacks = false;
static bool                 g_WantUpdateMonitors = true;

// Chain GLFW callbacks for main viewport: our callbacks will call the user's previously installed callbacks, if any.
static GLFWmousebuttonfun   g_PrevUserCallbackMousebutton = NULL;
static GLFWscrollfun        g_PrevUserCallbackScroll = NULL;
static GLFWkeyfun           g_PrevUserCallbackKey = NULL;
static GLFWcharfun          g_PrevUserCallbackChar = NULL;
static GLFWmonitorfun       g_PrevUserCallbackMonitor = NULL;
static GLFWcursorposfun     g_UserCallbackCursorPos = NULL;

static void ImGui_ImplGlfw_UpdateMonitors();
static void ImGui_ImplGlfw_InitPlatformInterface();
static void ImGui_ImplBgfx_InitPlatformInterface();
static void ImGui_ImplBgfx_RenderWindow(ImGuiViewport* viewport, void*);
void ImGui_ImplGlfw_MonitorCallback(GLFWmonitor*, int);

void imguiSetUserMousePosCallback(void(*callback)(void*, double, double))
{
	g_UserCallbackCursorPos = (GLFWcursorposfun)callback;
}

bgfx::ProgramHandle getShaderProgram()
{
	return imguiProgram;
}

void imguiInit(Window* window)
{
	g_Window = (GLFWwindow*)window->GetGLFWWindow();

	unsigned char* data;
	int width, height;
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Setup vertex declaration
	imguiVertexLayout
		.begin()
		.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();

	// Create font
	io.Fonts->AddFontDefault();
	io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);
	imguiFontTexture = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::BGRA8, 0, bgfx::copy(data, width * height * 4));
	imguiFontUniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

	// Create shader program
	
	bgfx::ShaderHandle vs = bgfx::createShader(bgfx::makeRef(vs_ocornut_imgui(), vs_ocornut_imgui_len()));
	bgfx::ShaderHandle fs = bgfx::createShader(bgfx::makeRef(fs_ocornut_imgui(), fs_ocornut_imgui_len()));
	imguiProgram = bgfx::createProgram(vs, fs, true);

	// Setup back-end capabilities flags
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
	io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;

	// Key mapping
	io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

	io.SetClipboardTextFn = imguiSetClipboardText;
	io.GetClipboardTextFn = imguiGetClipboardText;
	io.ClipboardUserData = g_Window;
	#if BX_PLATFORM_WINDOWS
	io.ImeWindowHandle = (void*)glfwGetWin32Window(g_Window);
	#endif

	g_MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);   // FIXME: GLFW doesn't have this.
	g_MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
	g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
	g_MouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

	g_PrevUserCallbackMousebutton = NULL;
	g_PrevUserCallbackScroll = NULL;
	g_PrevUserCallbackKey = NULL;
	g_PrevUserCallbackChar = NULL;
	g_PrevUserCallbackMonitor = NULL;

	ImGui_ImplGlfw_UpdateMonitors();
	glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui_ImplGlfw_InitPlatformInterface();
		ImGui_ImplBgfx_InitPlatformInterface();
	}
}

void imguiReset(u16 width, u16 height)
{
	bgfx::setViewRect(200, 0, 0, width, height);
	//bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000);
}

static void ImGui_ImplGlfw_UpdateMousePosAndButtons()
{
	// Update buttons
	ImGuiIO& io = ImGui::GetIO();
	for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
	{
		// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
		io.MouseDown[i] = g_MouseJustPressed[i] || glfwGetMouseButton(g_Window, i) != 0;
		g_MouseJustPressed[i] = false;
	}

	// Update mouse position
	const ImVec2 mouse_pos_backup = io.MousePos;
	io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	io.MouseHoveredViewport = 0;
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	for (int n = 0; n < platform_io.Viewports.Size; n++)
	{
		ImGuiViewport* viewport = platform_io.Viewports[n];
		GLFWwindow* window = (GLFWwindow*)viewport->PlatformHandle;
		IM_ASSERT(window != NULL);
		#ifdef __EMSCRIPTEN__
		const bool focused = true;
		IM_ASSERT(platform_io.Viewports.Size == 1);
		#else
		const bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
		#endif
		if (focused)
		{
			if (io.WantSetMousePos)
			{
				glfwSetCursorPos(window, (double)((double)mouse_pos_backup.x - (double)viewport->Pos.x), (double)((double)mouse_pos_backup.y - (double)viewport->Pos.y));
			} else
			{
				double mouse_x, mouse_y;
				glfwGetCursorPos(window, &mouse_x, &mouse_y);
				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					// Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
					int window_x, window_y;
					glfwGetWindowPos(window, &window_x, &window_y);
					io.MousePos = ImVec2((float)mouse_x + window_x, (float)mouse_y + window_y);
				} else
				{
					// Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
					io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
				}
			}
			for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
				io.MouseDown[i] |= glfwGetMouseButton(window, i) != 0;
		}

		// (Optional) When using multiple viewports: set io.MouseHoveredViewport to the viewport the OS mouse cursor is hovering.
		// Important: this information is not easy to provide and many high-level windowing library won't be able to provide it correctly, because
		// - This is _ignoring_ viewports with the ImGuiViewportFlags_NoInputs flag (pass-through windows).
		// - This is _regardless_ of whether another viewport is focused or being dragged from.
		// If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, imgui will ignore this field and infer the information by relying on the
		// rectangles and last focused time of every viewports it knows about. It will be unaware of other windows that may be sitting between or over your windows.
		// [GLFW] FIXME: This is currently only correct on Win32. See what we do below with the WM_NCHITTEST, missing an equivalent for other systems.
		// See https://github.com/glfw/glfw/issues/1236 if you want to help in making this a GLFW feature.
		#if 1 || (1 && defined(_WIN32))
		const bool window_no_input = (viewport->Flags & ImGuiViewportFlags_NoInputs) != 0;
		#if 1
		glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, window_no_input);
		#endif
		if (glfwGetWindowAttrib(window, GLFW_HOVERED) && !window_no_input)
			io.MouseHoveredViewport = viewport->ID;
		#endif
	}
}

void imguiEvents(float dt)
{
	ImGuiIO& io = ImGui::GetIO();

	// Setup display size
	int w, h;
	int displayW, displayH;
	glfwGetWindowSize(g_Window, &w, &h);
	glfwGetFramebufferSize(g_Window, &displayW, &displayH);
	io.DisplaySize = ImVec2((float)w, (float)h);
	io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)displayW / w) : 0, h > 0 ? ((float)displayH / h) : 0);

	// Setup time step
	io.DeltaTime = dt;

	// Update mouse position
	ImGui_ImplGlfw_UpdateMousePosAndButtons();

	// Update mouse cursor
	if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) && glfwGetInputMode(g_Window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
	{
		ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
		if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
		{
			// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
			glfwSetInputMode(g_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		} else
		{
			// Show OS mouse cursor
			glfwSetCursor(g_Window, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
			glfwSetInputMode(g_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
}

void imguiRender(ImDrawData* drawData, uint16_t vid)
{
	int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
	int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0)
		return;

	float L = drawData->DisplayPos.x;
	float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	float T = drawData->DisplayPos.y;
	float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

	glm::mat4 ortho_projection = glm::ortho(L, R, B, T);
	const float identity[4][4] =
	{
		{ 1.0f,   0.0f,   0.0f,   0.0f },
		{ 0.0f,   1.0f,   0.0f,   0.0f },
		{ 0.0f,   0.0f,   1.0f,   0.0f },
		{ 0.0f,   0.0f,   0.0f,   1.0f },
	};
	bgfx::setViewTransform(vid, &identity[0][0], &identity[0][0]);

	for (int ii = 0, num = drawData->CmdListsCount; ii < num; ++ii)
	{
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;

		const ImDrawList* drawList = drawData->CmdLists[ii];
		uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
		uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();

		if (!bgfx::getAvailTransientVertexBuffer(numVertices, imguiVertexLayout) || !bgfx::getAvailTransientIndexBuffer(numIndices))
		{
			break;
		}

		bgfx::allocTransientVertexBuffer(&tvb, numVertices, imguiVertexLayout);
		bgfx::allocTransientIndexBuffer(&tib, numIndices, true);

		ImDrawVert* verts = (ImDrawVert*)tvb.data;
		memcpy(verts, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));

		ImDrawIdx* indices = (ImDrawIdx*)tib.data;
		memcpy(indices, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

		uint32_t offset = 0;
		for (const ImDrawCmd* cmd = drawList->CmdBuffer.begin(), *cmdEnd = drawList->CmdBuffer.end(); cmd != cmdEnd; ++cmd)
		{
			if (cmd->UserCallback)
			{
				cmd->UserCallback(drawList, cmd);
			}
			else if (0 != cmd->ElemCount)
			{
				uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;
				bgfx::TextureHandle th = imguiFontTexture;
				if (cmd->TextureId != NULL)
				{
					th.idx = uint16_t(uintptr_t(cmd->TextureId));
				}
				state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
				
				ImVec4 clip_rect;
				clip_rect.x = (cmd->ClipRect.x - drawData->DisplayPos.x) * drawData->FramebufferScale.x;
				clip_rect.y = (cmd->ClipRect.y - drawData->DisplayPos.y) * drawData->FramebufferScale.y;
				clip_rect.z = (cmd->ClipRect.z - drawData->DisplayPos.x) * drawData->FramebufferScale.x;
				clip_rect.w = (cmd->ClipRect.w - drawData->DisplayPos.y) * drawData->FramebufferScale.y;

				//const uint16_t xx = uint16_t(max(cmd->ClipRect.x - drawData->DisplayPos.x, 0.0f));
				//const uint16_t yy = uint16_t(max(cmd->ClipRect.y - drawData->DisplayPos.y, 0.0f));
				//bgfx::setScissor(clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w);
				if (clip_rect.x < drawData->DisplaySize.x * drawData->FramebufferScale.x && clip_rect.y < drawData->DisplaySize.y * drawData->FramebufferScale.y && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
				{
					bgfx::setScissor(clip_rect.x, clip_rect.y, clip_rect.z - clip_rect.x, clip_rect.w - clip_rect.y);
					//bgfx::setScissor(cmd->ClipRect.x - drawData->DisplayPos.x, cmd->ClipRect.y - drawData->DisplayPos.y, cmd->ClipRect.z - drawData->DisplayPos.x, cmd->ClipRect.w - drawData->DisplayPos.y);
					glm::mat4 model = glm::mat4(1.0f);
					model = glm::translate(model, { -2.0f * (float)drawData->DisplayPos.x / (float)drawData->DisplaySize.x, 2.0f * (float)drawData->DisplayPos.y / (float)drawData->DisplaySize.y, 0.0f });
					bgfx::setTransform(&model[0][0]);
					bgfx::setState(state);
					bgfx::setTexture(0, imguiFontUniform, th);
					bgfx::setVertexBuffer(0, &tvb, 0, numVertices);
					bgfx::setIndexBuffer(&tib, offset, cmd->ElemCount);
					bgfx::submit(vid, imguiProgram);
				}
			}

			offset += cmd->ElemCount;
		}
	}
}

void imguiShutdown()
{
	for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
	{
		glfwDestroyCursor(g_MouseCursors[cursor_n]);
		g_MouseCursors[cursor_n] = NULL;
	}

	bgfx::destroy(imguiFontUniform);
	bgfx::destroy(imguiFontTexture);
	bgfx::destroy(imguiProgram);
	ImGui::DestroyPlatformWindows();
	ImGui::DestroyContext();
}

const char* imguiGetClipboardText(void* userData)
{
	return glfwGetClipboardString((GLFWwindow*)userData);
}

void imguiSetClipboardText(void* userData, const char* text)
{
	glfwSetClipboardString((GLFWwindow*)userData, text);
}

void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();

	if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(g_MouseJustPressed))
	{
		g_MouseJustPressed[button] = true;
		io.MouseDown[button] = true;
	}
	else if (action == GLFW_RELEASE && button >= 0 && button < IM_ARRAYSIZE(g_MouseJustPressed))
	{
		io.MouseDown[button] = false;
	}
	
	if (g_PrevUserCallbackMousebutton != NULL /*&& g_Window == window */ /*&& !io.WantCaptureMouse*/)
		g_PrevUserCallbackMousebutton(window, button, action, mods);
	
}

void ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (g_PrevUserCallbackScroll != NULL && window == g_Window)
		g_PrevUserCallbackScroll(window, xoffset, yoffset);

	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += (float)xoffset;
	io.MouseWheel += (float)yoffset;
}

void ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (g_PrevUserCallbackKey != NULL && window == g_Window)
		g_PrevUserCallbackKey(window, key, scancode, action, mods);

	ImGuiIO& io = ImGui::GetIO();
	if (action == GLFW_PRESS)
		io.KeysDown[key] = true;
	if (action == GLFW_RELEASE)
		io.KeysDown[key] = false;

	// Modifiers are not reliable across systems
	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	#ifdef _WIN32
	io.KeySuper = false;
	#else
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
	#endif
}

void ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c)
{
	if (g_PrevUserCallbackChar != NULL && window == g_Window)
		g_PrevUserCallbackChar(window, c);

	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(c);
}

void ImGui_ImplGlfw_MonitorCallback(GLFWmonitor*, int)
{
	g_WantUpdateMonitors = true;
}

void imguiInstallCallbacks()
{
	g_InstalledCallbacks = true;
	g_PrevUserCallbackMousebutton = glfwSetMouseButtonCallback(g_Window, ImGui_ImplGlfw_MouseButtonCallback);
	g_PrevUserCallbackScroll = glfwSetScrollCallback(g_Window, ImGui_ImplGlfw_ScrollCallback);
	g_PrevUserCallbackKey = glfwSetKeyCallback(g_Window, ImGui_ImplGlfw_KeyCallback);
	g_PrevUserCallbackChar = glfwSetCharCallback(g_Window, ImGui_ImplGlfw_CharCallback);
	g_PrevUserCallbackMonitor = glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);
}

// Folowing mostly from imgui_impl_glfw.cpp

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGuiViewportDataGlfw
{
	GLFWwindow* Window;
	bool        WindowOwned;
	int         IgnoreWindowPosEventFrame;
	int         IgnoreWindowSizeEventFrame;

	ImGuiViewportDataGlfw() { Window = NULL; WindowOwned = false; IgnoreWindowSizeEventFrame = IgnoreWindowPosEventFrame = -1; }
	~ImGuiViewportDataGlfw() { IM_ASSERT(Window == NULL); }
};

struct ImGuiViewportDataBgfx
{
	uint16_t vid;
	bgfx::FrameBufferHandle fbh;

	ImGuiViewportDataBgfx() { vid = NULL; }
	~ImGuiViewportDataBgfx() { ; }
};

static void ImGui_ImplGlfw_WindowCloseCallback(GLFWwindow* window)
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
		viewport->PlatformRequestClose = true;
}

// GLFW may dispatch window pos/size events after calling glfwSetWindowPos()/glfwSetWindowSize().
// However: depending on the platform the callback may be invoked at different time:
// - on Windows it appears to be called within the glfwSetWindowPos()/glfwSetWindowSize() call
// - on Linux it is queued and invoked during glfwPollEvents()
// Because the event doesn't always fire on glfwSetWindowXXX() we use a frame counter tag to only
// ignore recent glfwSetWindowXXX() calls.
static void ImGui_ImplGlfw_WindowPosCallback(GLFWwindow* window, int, int)
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
	{
		if (ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData)
		{
			bool ignore_event = (ImGui::GetFrameCount() <= data->IgnoreWindowPosEventFrame + 1);
			//data->IgnoreWindowPosEventFrame = -1;
			if (ignore_event)
				return;
		}
		viewport->PlatformRequestMove = true;
	}
}

static void ImGui_ImplGlfw_WindowSizeCallback(GLFWwindow* window, int width, int height)
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
	{
		if (ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData)
		{
			bool ignore_event = (ImGui::GetFrameCount() <= data->IgnoreWindowSizeEventFrame + 1);
			//data->IgnoreWindowSizeEventFrame = -1;
			if (ignore_event)
				return;
		}
		viewport->PlatformRequestResize = true;
	}
}

static void ImGui_ImplGlfw_CreateWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = IM_NEW(ImGuiViewportDataGlfw)();
	viewport->PlatformUserData = data;

	// GLFW 3.2 unfortunately always set focus on glfwCreateWindow() if GLFW_VISIBLE is set, regardless of GLFW_FOCUSED
	// With GLFW 3.3, the hint GLFW_FOCUS_ON_SHOW fixes this problem
	glfwWindowHint(GLFW_VISIBLE, false);
	glfwWindowHint(GLFW_FOCUSED, false);
	glfwWindowHint(GLFW_RESIZABLE, true);
	#if GLFW_HAS_FOCUS_ON_SHOW
	glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);
	#endif
	glfwWindowHint(GLFW_DECORATED, (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? false : true);
	#if GLFW_HAS_WINDOW_TOPMOST
	glfwWindowHint(GLFW_FLOATING, (viewport->Flags & ImGuiViewportFlags_TopMost) ? true : false);
	#endif
	GLFWwindow* share_window = (bgfx::getRendererType() == bgfx::RendererType::OpenGL) ? g_Window : NULL;
	data->Window = glfwCreateWindow((int)viewport->Size.x, (int)viewport->Size.y, "No Title Yet", NULL, share_window);
	data->WindowOwned = true;
	viewport->PlatformHandle = (void*)data->Window;
	#ifdef _WIN32
	viewport->PlatformHandleRaw = glfwGetWin32Window(data->Window);
	#endif
	glfwSetWindowPos(data->Window, (int)viewport->Pos.x, (int)viewport->Pos.y);

	// Install GLFW callbacks for secondary viewports
	//glfwSetMouseButtonCallback(data->Window, (GLFWmousebuttonfun*)[](GLFWwindow* window, int button, int action, int mods){ ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods); })
	glfwSetMouseButtonCallback(data->Window, ImGui_ImplGlfw_MouseButtonCallback);
	glfwSetScrollCallback(data->Window, ImGui_ImplGlfw_ScrollCallback);
	glfwSetKeyCallback(data->Window, ImGui_ImplGlfw_KeyCallback);
	glfwSetCharCallback(data->Window, ImGui_ImplGlfw_CharCallback);
	glfwSetWindowCloseCallback(data->Window, ImGui_ImplGlfw_WindowCloseCallback);
	glfwSetWindowPosCallback(data->Window, ImGui_ImplGlfw_WindowPosCallback);
	glfwSetWindowSizeCallback(data->Window, ImGui_ImplGlfw_WindowSizeCallback);

	if (g_UserCallbackCursorPos)
	{
		glfwSetCursorPosCallback(data->Window, g_UserCallbackCursorPos);
	}

	if (bgfx::getRendererType() == bgfx::RendererType::OpenGL)
	{
		glfwMakeContextCurrent(data->Window);
		glfwSwapInterval(0);
	}
}

static void ImGui_ImplGlfw_DestroyWindow(ImGuiViewport* viewport)
{
	if (ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData)
	{
		if (data->WindowOwned)
		{
			#if !GLFW_HAS_MOUSE_PASSTHROUGH && GLFW_HAS_WINDOW_HOVERED && defined(_WIN32)
			HWND hwnd = (HWND)viewport->PlatformHandleRaw;
			::RemovePropA(hwnd, "IMGUI_VIEWPORT");
			#endif
			glfwDestroyWindow(data->Window);
		}
		data->Window = NULL;
		IM_DELETE(data);
	}
	viewport->PlatformUserData = viewport->PlatformHandle = NULL;
}

// We have submitted https://github.com/glfw/glfw/pull/1568 to allow GLFW to support "transparent inputs".
// In the meanwhile we implement custom per-platform workarounds here (FIXME-VIEWPORT: Implement same work-around for Linux/OSX!)
#if !GLFW_HAS_MOUSE_PASSTHROUGH && GLFW_HAS_WINDOW_HOVERED && defined(_WIN32)
static WNDPROC g_GlfwWndProc = NULL;
static LRESULT CALLBACK WndProcNoInputs(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCHITTEST)
	{
		// Let mouse pass-through the window. This will allow the backend to set io.MouseHoveredViewport properly (which is OPTIONAL).
		// The ImGuiViewportFlags_NoInputs flag is set while dragging a viewport, as want to detect the window behind the one we are dragging.
		// If you cannot easily access those viewport flags from your windowing/event code: you may manually synchronize its state e.g. in
		// your main loop after calling UpdatePlatformWindows(). Iterate all viewports/platform windows and pass the flag to your windowing system.
		ImGuiViewport* viewport = (ImGuiViewport*)::GetPropA(hWnd, "IMGUI_VIEWPORT");
		if (viewport->Flags & ImGuiViewportFlags_NoInputs)
			return HTTRANSPARENT;
	}
	return ::CallWindowProc(g_GlfwWndProc, hWnd, msg, wParam, lParam);
}
#endif

static void ImGui_ImplGlfw_ShowWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	
	#if defined(_WIN32)
	// GLFW hack: Hide icon from task bar
	HWND hwnd = (HWND)viewport->PlatformHandleRaw;
	if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
	{
		LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
		ex_style &= ~WS_EX_APPWINDOW;
		ex_style |= WS_EX_TOOLWINDOW;
		::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
	}

	// GLFW hack: install hook for WM_NCHITTEST message handler
	#if !GLFW_HAS_MOUSE_PASSTHROUGH && GLFW_HAS_WINDOW_HOVERED && defined(_WIN32)
	::SetPropA(hwnd, "IMGUI_VIEWPORT", viewport);
	if (g_GlfwWndProc == NULL)
		g_GlfwWndProc = (WNDPROC)::GetWindowLongPtr(hwnd, GWLP_WNDPROC);
	::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcNoInputs);
	#endif

	#if !GLFW_HAS_FOCUS_ON_SHOW
	// GLFW hack: GLFW 3.2 has a bug where glfwShowWindow() also activates/focus the window.
	// The fix was pushed to GLFW repository on 2018/01/09 and should be included in GLFW 3.3 via a GLFW_FOCUS_ON_SHOW window attribute.
	// See https://github.com/glfw/glfw/issues/1189
	// FIXME-VIEWPORT: Implement same work-around for Linux/OSX in the meanwhile.
	if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
	{
		::ShowWindow(hwnd, SW_SHOWNA);
		return;
	}
	#endif
	#endif

	glfwShowWindow(data->Window);
}

static ImVec2 ImGui_ImplGlfw_GetWindowPos(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	int x = 0, y = 0;
	glfwGetWindowPos(data->Window, &x, &y);
	return ImVec2((float)x, (float)y);
}

static void ImGui_ImplGlfw_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	data->IgnoreWindowPosEventFrame = ImGui::GetFrameCount();
	glfwSetWindowPos(data->Window, (int)pos.x, (int)pos.y);
}

static ImVec2 ImGui_ImplGlfw_GetWindowSize(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	int w = 0, h = 0;
	glfwGetWindowSize(data->Window, &w, &h);
	return ImVec2((float)w, (float)h);
}

static void ImGui_ImplGlfw_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	#if __APPLE__ && !GLFW_HAS_OSX_WINDOW_POS_FIX
	// Native OS windows are positioned from the bottom-left corner on macOS, whereas on other platforms they are
	// positioned from the upper-left corner. GLFW makes an effort to convert macOS style coordinates, however it
	// doesn't handle it when changing size. We are manually moving the window in order for changes of size to be based
	// on the upper-left corner.
	int x, y, width, height;
	glfwGetWindowPos(data->Window, &x, &y);
	glfwGetWindowSize(data->Window, &width, &height);
	glfwSetWindowPos(data->Window, x, y - height + size.y);
	#endif
	data->IgnoreWindowSizeEventFrame = ImGui::GetFrameCount();
	glfwSetWindowSize(data->Window, (int)size.x, (int)size.y);
}

static void ImGui_ImplGlfw_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	glfwSetWindowTitle(data->Window, title);
}

static void ImGui_ImplGlfw_SetWindowFocus(ImGuiViewport* viewport)
{
	#if GLFW_HAS_FOCUS_WINDOW
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	glfwFocusWindow(data->Window);
	#else
	// FIXME: What are the effect of not having this function? At the moment imgui doesn't actually call SetWindowFocus - we set that up ahead, will answer that question later.
	(void)viewport;
	#endif
}

static bool ImGui_ImplGlfw_GetWindowFocus(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	return glfwGetWindowAttrib(data->Window, GLFW_FOCUSED) != 0;
}

static bool ImGui_ImplGlfw_GetWindowMinimized(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	return glfwGetWindowAttrib(data->Window, GLFW_ICONIFIED) != 0;
}

#if GLFW_HAS_WINDOW_ALPHA
static void ImGui_ImplGlfw_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	glfwSetWindowOpacity(data->Window, alpha);
}
#endif

static void ImGui_ImplGlfw_RenderWindow(ImGuiViewport* viewport, void*)
{
	//ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	/*if (g_ClientApi == GlfwClientApi_OpenGL)
		glfwMakeContextCurrent(data->Window);*/
}

static void ImGui_ImplGlfw_SwapBuffers(ImGuiViewport* viewport, void*)
{
	//ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	/*if (g_ClientApi == GlfwClientApi_OpenGL)
	{
		glfwMakeContextCurrent(data->Window);
		glfwSwapBuffers(data->Window);
	}*/
}

#if GLFW_HAS_VULKAN
#ifndef VULKAN_H_
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif
VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR)
struct VkAllocationCallbacks;
enum VkResult { VK_RESULT_MAX_ENUM = 0x7FFFFFFF };
#endif // VULKAN_H_
extern "C" { extern GLFWAPI VkResult glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface); }
static int ImGui_ImplGlfw_CreateVkSurface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	IM_ASSERT(g_ClientApi == GlfwClientApi_Vulkan);
	VkResult err = glfwCreateWindowSurface((VkInstance)vk_instance, data->Window, (const VkAllocationCallbacks*)vk_allocator, (VkSurfaceKHR*)out_vk_surface);
	return (int)err;
}
#endif // GLFW_HAS_VULKAN


static void ImGui_ImplGlfw_InitPlatformInterface()
{
	// Register platform interface (will be coupled with a renderer interface)
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Platform_CreateWindow = ImGui_ImplGlfw_CreateWindow;
	platform_io.Platform_DestroyWindow = ImGui_ImplGlfw_DestroyWindow;
	platform_io.Platform_ShowWindow = ImGui_ImplGlfw_ShowWindow;
	platform_io.Platform_SetWindowPos = ImGui_ImplGlfw_SetWindowPos;
	platform_io.Platform_GetWindowPos = ImGui_ImplGlfw_GetWindowPos;
	platform_io.Platform_SetWindowSize = ImGui_ImplGlfw_SetWindowSize;
	platform_io.Platform_GetWindowSize = ImGui_ImplGlfw_GetWindowSize;
	platform_io.Platform_SetWindowFocus = ImGui_ImplGlfw_SetWindowFocus;
	platform_io.Platform_GetWindowFocus = ImGui_ImplGlfw_GetWindowFocus;
	platform_io.Platform_GetWindowMinimized = ImGui_ImplGlfw_GetWindowMinimized;
	platform_io.Platform_SetWindowTitle = ImGui_ImplGlfw_SetWindowTitle;
	platform_io.Platform_RenderWindow = ImGui_ImplGlfw_RenderWindow;
	platform_io.Platform_SwapBuffers = ImGui_ImplGlfw_SwapBuffers;
	#if GLFW_HAS_WINDOW_ALPHA
	platform_io.Platform_SetWindowAlpha = ImGui_ImplGlfw_SetWindowAlpha;
	#endif
	#if GLFW_HAS_VULKAN
	platform_io.Platform_CreateVkSurface = ImGui_ImplGlfw_CreateVkSurface;
	#endif
	#if HAS_WIN32_IME
	platform_io.Platform_SetImeInputPos = ImGui_ImplWin32_SetImeInputPos;
	#endif

	// Register main window handle (which is owned by the main application, not by us)
	// This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGuiViewportDataGlfw* data = IM_NEW(ImGuiViewportDataGlfw)();
	data->Window = g_Window;
	data->WindowOwned = false;
	main_viewport->PlatformUserData = data;
	main_viewport->PlatformHandle = (void*)g_Window;
}

static void ImGui_ImplGlfw_UpdateMonitors()
{
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	int monitors_count = 0;
	GLFWmonitor** glfw_monitors = glfwGetMonitors(&monitors_count);
	platform_io.Monitors.resize(0);
	for (int n = 0; n < monitors_count; n++)
	{
		ImGuiPlatformMonitor monitor;
		int x, y;
		glfwGetMonitorPos(glfw_monitors[n], &x, &y);
		const GLFWvidmode* vid_mode = glfwGetVideoMode(glfw_monitors[n]);
		monitor.MainPos = monitor.WorkPos = ImVec2((float)x, (float)y);
		monitor.MainSize = monitor.WorkSize = ImVec2((float)vid_mode->width, (float)vid_mode->height);
		#if GLFW_HAS_MONITOR_WORK_AREA
		int w, h;
		glfwGetMonitorWorkarea(glfw_monitors[n], &x, &y, &w, &h);
		if (w > 0 && h > 0) // Workaround a small GLFW issue reporting zero on monitor changes: https://github.com/glfw/glfw/pull/1761
		{
			monitor.WorkPos = ImVec2((float)x, (float)y);
			monitor.WorkSize = ImVec2((float)w, (float)h);
		}
		#endif
		#if GLFW_HAS_PER_MONITOR_DPI
		// Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at runtime.
		float x_scale, y_scale;
		glfwGetMonitorContentScale(glfw_monitors[n], &x_scale, &y_scale);
		monitor.DpiScale = x_scale;
		#endif
		platform_io.Monitors.push_back(monitor);
	}
	g_WantUpdateMonitors = false;
}

static void ImGui_ImplBgfx_RenderWindow(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataBgfx* data = (ImGuiViewportDataBgfx*)viewport->RendererUserData;
	imguiRender(viewport->DrawData, data->vid);
}

static void ImGui_ImplBgfx_CreateWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataBgfx* data = IM_NEW(ImGuiViewportDataBgfx)();
	viewport->RendererUserData = data;
	ImGuiViewportDataGlfw* glfw_data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	data->fbh = bgfx::createFrameBuffer(viewport->PlatformHandleRaw, viewport->Size.x, viewport->Size.y);
	data->vid = 201; // @HACK This might work. Please generate a proper unique view id.
	bgfx::setViewClear(201, BGFX_CLEAR_COLOR, 0x00000000);
	bgfx::setViewFrameBuffer(data->vid, data->fbh);
	bgfx::setViewRect(data->vid, 0, 0, viewport->Size.x, viewport->Size.y);
}

static void ImGui_ImplBgfx_DestroyWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataBgfx* data = (ImGuiViewportDataBgfx*)viewport->RendererUserData;
	if (data && bgfx::isValid(data->fbh))
		bgfx::destroy(data->fbh);
	if (data)
	{
		IM_DELETE(data);
		viewport->RendererUserData = NULL;
	}
}

static void ImGui_ImplBgfx_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiViewportDataBgfx* data = (ImGuiViewportDataBgfx*)viewport->RendererUserData;
	ImGuiViewportDataGlfw* glfw_data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;

	if (bgfx::isValid(data->fbh))
		bgfx::destroy(data->fbh);
	data->fbh = bgfx::createFrameBuffer(glfwGetWin32Window(glfw_data->Window), size.x, size.y);
	bgfx::setViewFrameBuffer(data->vid, data->fbh);
	bgfx::setViewRect(data->vid, 0, 0, viewport->Size.x, viewport->Size.y);
	
	if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
	{
		ImVec4 clear_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
		bgfx::setViewClear(data->vid, BGFX_CLEAR_COLOR, 0x000000FF);
	}
}

static void ImGui_ImplBgfx_InitPlatformInterface()
{
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Renderer_RenderWindow = ImGui_ImplBgfx_RenderWindow;
	platform_io.Renderer_CreateWindow = ImGui_ImplBgfx_CreateWindow;
	platform_io.Renderer_DestroyWindow = ImGui_ImplBgfx_DestroyWindow;
	platform_io.Renderer_SetWindowSize = ImGui_ImplBgfx_SetWindowSize;
}
