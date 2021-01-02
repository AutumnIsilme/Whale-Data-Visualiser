workspace "Whale Data Visualiser"
	architecture "x86_64"
	startproject "Visualiser"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags 
	{
		"MultiProcessorCompile"
	}
	
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "Visualiser/vendor/GLFW/include"
IncludeDir["glm"] = "Visualiser/vendor/glm"
IncludeDir["stb_image"] = "Visualiser/vendor/stb_image"
IncludeDir["spdlog"] = "Visualiser/vendor/spdlog/include"
IncludeDir["imgui"] = "Visualiser/vendor/imgui"
IncludeDir["implot"] = "Visualiser/vendor/implot"
IncludeDir["bgfx"] = "Visualiser/vendor/bgfx/include"
IncludeDir["bimg"] = "Visualiser/vendor/bimg/include"
IncludeDir["bx"] = "Visualiser/vendor/bx/include"
IncludeDir["nfd"] = "Visualiser/vendor/nativefiledialog/src/include"

group ""

project "Visualiser"
	location "Visualiser"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
    staticruntime "on"
    systemversion "latest"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",
		"%{prj.name}/vendor/glm/**.hpp",
		"%{prj.name}/vendor/glm/**.inl"
	}
	
	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}
	
	includedirs
	{
		"%{prj.name}/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.imgui}",
        "%{IncludeDir.implot}",
        "%{IncludeDir.bgfx}",
        "%{IncludeDir.bimg}",
        "%{IncludeDir.bx}",
        "%{IncludeDir.nfd}",
        "Visualiser/vendor/bx/include/compat/msvc"
	}
	
	links
	{
		"GLFW",
        "imgui",
        "implot",
        "bgfx",
        "bimg",
        "bx",
        "nativefiledialog"
	}
	
	filter "system:windows"
		systemversion "latest"
		
		defines
		{
            "GLFW_INCLUDE_NONE",
            "_PLATFORM_WINDOWS"
        }
        
        links
        {
            "comctl32.lib"
        }
		
		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
		}
		
	filter "configurations:Debug"
		defines "_DEBUG"
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines "NDEBUG"
		runtime "Release"
		optimize "on"
	
	filter "configurations:Dist"
		defines {
            "NDEBUG",
            "DIST"
        }
		runtime "Release"
		optimize "on"


group "Dependencies"
	include "Visualiser/vendor/GLFW"
    include "Visualiser/vendor/Glad"

local IMGUI_DIR = "Visualiser/vendor/imgui"

project "ImGui"
    kind "StaticLib"
    language "C++"
    
	targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
        path.join(IMGUI_DIR, "imconfig.h"),
        path.join(IMGUI_DIR, "imgui.h"),
        path.join(IMGUI_DIR, "imgui.cpp"),
        path.join(IMGUI_DIR, "imgui_draw.cpp"),
        path.join(IMGUI_DIR, "imgui_internal.h"),
        path.join(IMGUI_DIR, "imgui_tables.cpp"),
        path.join(IMGUI_DIR, "imgui_widgets.cpp"),
        path.join(IMGUI_DIR, "imstb_rectpack.h"),
        path.join(IMGUI_DIR, "imstb_textedit.h"),
        path.join(IMGUI_DIR, "imstb_truetype.h"),
        path.join(IMGUI_DIR, "imgui_demo.cpp")
    }
    
	filter "system:windows"
        systemversion "latest"
        cppdialect "C++17"
        staticruntime "On"

local IMPLOT_DIR = "Visualiser/vendor/implot"

project "implot"
    kind "StaticLib"
    language "C++"

    targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        path.join(IMPLOT_DIR, "implot.h"),
        path.join(IMPLOT_DIR, "implot_internal.h"),
        path.join(IMPLOT_DIR, "implot.cpp"),
        path.join(IMPLOT_DIR, "implot_items.cpp")
    }

    includedirs
    {
        "%{IncludeDir.imgui}"
    }

    filter "system:windows"
        systemversion "latest"
        cppdialect "C++17"
        staticruntime "On"

local NFD_DIR = "Visualiser/vendor/nativefiledialog/src"

project "nativefiledialog"
    kind "StaticLib"
    language "C++"

    targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        path.join(NFD_DIR, "common.h"),
        path.join(NFD_DIR, "include/nfd.h"),
        path.join(NFD_DIR, "nfd_common.h"),
        path.join(NFD_DIR, "nfd_common.c"),
        path.join(NFD_DIR, "nfd_win.cpp")
    }

    includedirs
    {
        "Visualiser/vendor/nativefiledialog/src/include"
    }

    filter "system:windows"
        systemversion "latest"
        cppdialect "C++17"
        staticruntime "On"

--[[

    Below is borrowed from https://github.com/jpcy/bgfx-minimal-example under the BSD 2-clause license:

Copyright 2010-2019 Branimir Karadzic

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

]]

local BGFX_DIR = "Visualiser/vendor/bgfx"
local BIMG_DIR = "Visualiser/vendor/bimg"
local BX_DIR = "Visualiser/vendor/bx"

function setBxCompat()
	filter "action:vs*"
		includedirs { path.join(BX_DIR, "include/compat/msvc") }
	filter { "system:windows", "action:gmake" }
		includedirs { path.join(BX_DIR, "include/compat/mingw") }
	filter { "system:macosx" }
		includedirs { path.join(BX_DIR, "include/compat/osx") }
		buildoptions { "-x objective-c++" }
end
        
project "bgfx"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	exceptionhandling "Off"
    rtti "Off"
    systemversion "latest"
    staticruntime "on"
	defines "__STDC_FORMAT_MACROS"
	files
	{
		path.join(BGFX_DIR, "include/bgfx/**.h"),
		path.join(BGFX_DIR, "src/*.cpp"),
		path.join(BGFX_DIR, "src/*.h"),
	}
	excludes
	{
		path.join(BGFX_DIR, "src/amalgamated.cpp"),
	}
	includedirs
	{
		path.join(BX_DIR, "include"),
		path.join(BIMG_DIR, "include"),
		path.join(BGFX_DIR, "include"),
		path.join(BGFX_DIR, "3rdparty"),
		path.join(BGFX_DIR, "3rdparty/dxsdk/include"),
		path.join(BGFX_DIR, "3rdparty/khronos")
	}
	filter "configurations:Debug"
        defines "BGFX_CONFIG_DEBUG=1"
        runtime "Debug"
		symbols "on"
	filter "action:vs*"
		defines "_CRT_SECURE_NO_WARNINGS"
		excludes
		{
			path.join(BGFX_DIR, "src/glcontext_glx.cpp"),
			path.join(BGFX_DIR, "src/glcontext_egl.cpp")
		}
	filter "system:macosx"
		files
		{
			path.join(BGFX_DIR, "src/*.mm"),
		}
	setBxCompat()

project "bimg"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	exceptionhandling "Off"
    rtti "Off"
    systemversion "latest"
    staticruntime "on"
	files
	{
		path.join(BIMG_DIR, "include/bimg/*.h"),
		path.join(BIMG_DIR, "src/image.cpp"),
		path.join(BIMG_DIR, "src/image_gnf.cpp"),
		path.join(BIMG_DIR, "src/*.h"),
		path.join(BIMG_DIR, "3rdparty/astc-codec/src/decoder/*.cc")
	}
	includedirs
	{
		path.join(BX_DIR, "include"),
		path.join(BIMG_DIR, "include"),
		path.join(BIMG_DIR, "3rdparty/astc-codec"),
		path.join(BIMG_DIR, "3rdparty/astc-codec/include"),
	}
    setBxCompat()
    filter "Configurations:Debug"
        runtime "Debug"
        symbols "on"

project "bx"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	exceptionhandling "Off"
    rtti "Off"
    systemversion "latest"
    staticruntime "on"
	defines "__STDC_FORMAT_MACROS"
	files
	{
		path.join(BX_DIR, "include/bx/*.h"),
		path.join(BX_DIR, "include/bx/inline/*.inl"),
		path.join(BX_DIR, "src/*.cpp")
	}
	excludes
	{
		path.join(BX_DIR, "src/amalgamated.cpp"),
		path.join(BX_DIR, "src/crtnone.cpp")
	}
	includedirs
	{
		path.join(BX_DIR, "3rdparty"),
		path.join(BX_DIR, "include")
	}
	filter "action:vs*"
		defines "_CRT_SECURE_NO_WARNINGS"
    setBxCompat()
    filter "Configurations:Debug"
        runtime "Debug"
        symbols "on"
    filter "system:windows"
        includedirs {
            path.join(BX_DIR, "include/compat/msvc")
        }
