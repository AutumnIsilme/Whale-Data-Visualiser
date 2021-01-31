#pragma once

#include "Window.h"

#include <imgui.h>
#include <bgfx/bgfx.h>

void imguiInit(Window* window);
void imguiInstallCallbacks();
void imguiReset(u16 width, u16 height);
void imguiEvents(float dt);
void imguiRender(ImDrawData* drawData, unsigned short vid = 200);
void imguiShutdown();
const char* imguiGetClipboardText(void* userData);
void imguiSetClipboardText(void* userData, const char* text);
void imguiSetUserMousePosCallback(void(*callback)(void*, double, double));
bgfx::ProgramHandle	getShaderProgram();

#define IMGUI_TEXTURE_FROM_BGFX(x) ((void*)((x).idx))