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
IncludeDir["GLAD"] = "Visualiser/vendor/GLAD/include"
IncludeDir["glm"] = "Visualiser/vendor/glm"
IncludeDir["stb_image"] = "Visualiser/vendor/stb_image"

group "Dependencies"
	include "Visualiser/vendor/GLFW"
	include "Visualiser/vendor/Glad"

group ""

project "Visualiser"
	location "Visualiser"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	pchheader "pch.h"
	pchsource "Visualiser/src/pch.cpp"
	
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
		"%{IncludeDir.GLAD}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}"
	}
	
	links
	{
		"GLFW",
		"GLAD",
		"opengl32.lib"
	}
	
	filter "system:windows"
		systemversion "latest"
		
		defines
		{
            "GLFW_INCLUDE_NONE",
            "_PLATFORM_WINDOWS"
		}
		
		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
		}
		
	filter "configurations:Debug"
		defines "DEBUG"
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

        --[[
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs
	{
		"Supernova/src",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.glm}",
		"Supernova/vendor"
	}
	
	links
	{
		"Supernova"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"
		
	filter "configurations:Debug"
		defines "SN_DEBUG"
		symbols "on"
		
	filter "configurations:Release"
		defines "SN_RELEASE"
		optimize "on"
	
	filter "configurations:Dist"
		defines "SN_DIST"
        optimize "on"
        ]]