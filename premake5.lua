-------------------------------------------------------------------------------
-- Premake5 build script for Quake 2
-------------------------------------------------------------------------------

-- Convenience locals
local conf_dbg = "debug"
local conf_rel = "release"
local conf_rtl = "retail"

local plat_32bit = "x86"
local plat_64bit = "x64"

local build_dir = "../build"

local filter_dbg = "configurations:" .. conf_dbg
local filter_rel_or_rtl = "configurations:release or retail" -- TODO: This shouldn't be necessary
local filter_rtl = "configurations:" .. conf_rtl

local filter_32bit = "platforms:" .. plat_32bit
local filter_64bit = "platforms:" .. plat_64bit

-- Workspace definition -------------------------------------------------------

workspace "quake2"
	configurations { conf_dbg, conf_rel, conf_rtl }
	platforms { plat_32bit, plat_64bit }
	location( build_dir )
	preferredtoolarchitecture "x86_64"
	startproject "engine"

-- Configuration --------------------------------------------------------------

-- Misc flags for all projects

includedirs { "thirdparty/stb" }

flags { "MultiProcessorCompile", "NoBufferSecurityCheck" }
staticruntime "On"
cppdialect "C++20"
warnings "Default"
floatingpoint "Fast"
characterset "ASCII"
exceptionhandling "Off"

-- Config for all 32-bit projects
filter( filter_32bit )
	vectorextensions "SSE2"
	architecture "x86"
filter {}
	
-- Config for all 64-bit projects
filter( filter_64bit )
	architecture "x86_64"
filter {}

-- Config for Windows
filter "system:windows"
	buildoptions { "/utf-8", "/permissive", "/Zc:__cplusplus" }
	defines { "WIN32", "_WINDOWS", "_CRT_SECURE_NO_WARNINGS" }
filter {}
	
-- Config for Windows, release, clean this up!
filter { "system:windows", filter_rel_or_rtl }
	buildoptions { "/Gw" }
filter {}

-- Config for all projects in debug, _DEBUG is defined for library-compatibility
filter( filter_dbg )
	defines { "_DEBUG", "Q_DEBUG" }
	symbols "FastLink"
filter {}

-- Config for all projects in release or retail, NDEBUG is defined for cstd compatibility (assert)
filter( filter_rel_or_rtl )
	defines { "NDEBUG", "Q_RELEASE" }
	symbols "Full"
	optimize "Speed"
filter {}

-- Config for all projects in retail
filter( filter_rtl )
	defines { "Q_RETAIL" }
	symbols "Off"
	flags { "LinkTimeOptimization" }
filter {}

-- Config for shared library projects
filter "kind:SharedLib"
	flags { "NoManifest" } -- We don't want manifests for DLLs
filter {}

-- Project definitions --------------------------------------------------------

local zlib_public = {
	"thirdparty/zlib/zconf.h",
	"thirdparty/zlib/zlib.h"
}

local zlib_sources = {
	"thirdparty/zlib/adler32.c",
	"thirdparty/zlib/compress.c",
	"thirdparty/zlib/crc32.c",
	"thirdparty/zlib/crc32.h",
	"thirdparty/zlib/deflate.c",
	"thirdparty/zlib/deflate.h",
	"thirdparty/zlib/gzclose.c",
	"thirdparty/zlib/gzguts.h",
	"thirdparty/zlib/gzlib.c",
	"thirdparty/zlib/gzread.c",
	"thirdparty/zlib/gzwrite.c",
	"thirdparty/zlib/infback.c",
	"thirdparty/zlib/inffast.c",
	"thirdparty/zlib/inffast.h",
	"thirdparty/zlib/inffixed.h",
	"thirdparty/zlib/inflate.c",
	"thirdparty/zlib/inflate.h",
	"thirdparty/zlib/inftrees.c",
	"thirdparty/zlib/inftrees.h",
	"thirdparty/zlib/trees.c",
	"thirdparty/zlib/trees.h",
	"thirdparty/zlib/uncompr.c",
	"thirdparty/zlib/zutil.c",
	"thirdparty/zlib/zutil.h"
}

local libpng_public = {
	"thirdparty/libpng/png.h",
	"thirdparty/libpng/pngconf.h"
}

local libpng_sources = {
	"thirdparty/libpng/png.c",
	"thirdparty/libpng/pngpriv.h",
	"thirdparty/libpng/pngstruct.h",
	"thirdparty/libpng/pnginfo.h",
	"thirdparty/libpng/pngdebug.h",
	"thirdparty/libpng/pngerror.c",
	"thirdparty/libpng/pngget.c",
	"thirdparty/libpng/pngmem.c",
	"thirdparty/libpng/pngpread.c",
	"thirdparty/libpng/pngread.c",
	"thirdparty/libpng/pngrio.c",
	"thirdparty/libpng/pngrtran.c",
	"thirdparty/libpng/pngrutil.c",
	"thirdparty/libpng/pngset.c",
	"thirdparty/libpng/pngtrans.c",
	"thirdparty/libpng/pngwio.c",
	"thirdparty/libpng/pngwrite.c",
	"thirdparty/libpng/pngwtran.c",
	"thirdparty/libpng/pngwutil.c"
}

local glew_public = {
	"thirdparty/glew/include/GL/glew.h",
	"thirdparty/glew/include/GL/wglew.h"
}

local glew_sources = {
	"thirdparty/glew/src/glew.c"
}

local xatlas_public = {
	"thirdparty/xatlas/xatlas.h",
}

local xatlas_sources = {
	"thirdparty/xatlas/xatlas.cpp",
}

local uvatlas_public = {
	"thirdparty/UVAtlas/UVAtlas/inc/UVAtlas.h",
}

local uvatlas_sources = {
	"thirdparty/UVAtlas/UVAtlas/geodesics/*.cpp",
	"thirdparty/UVAtlas/UVAtlas/geodesics/*.h",
	"thirdparty/UVAtlas/UVAtlas/isochart/*.cpp",
	"thirdparty/UVAtlas/UVAtlas/isochart/*.h"
}

-------------------------------------------------------------------------------

group "game"

project "engine"
	kind "WindowedApp"
	targetname "q2game"
	language "C++"
	targetdir "../game"
	includedirs { "thirdparty/UVAtlas/UVAtlas/inc", "thirdparty/xatlas", "thirdparty/glew/include", "thirdparty/zlib", "thirdparty/libpng", "thirdparty/libpng_config" }
	defines { "GLEW_STATIC", "GLEW_NO_GLU" }
	filter "system:windows"
		linkoptions { "/ENTRY:mainCRTStartup" }
		links { "ws2_32", "dsound", "dxguid", "opengl32", "noenv.obj", "zlib", "libpng", "uvatlas" }
	filter {}
	filter "system:linux"
		links { "sdl2" }
	filter {}
	
	disablewarnings { "4244", "4267" }

	files {
		"common/*",
		"engine/client/*",
		"engine/server/*",
		"engine/res/*",
		"engine/shared/*",
		
		"engine/renderer/*",
		
		"game_shared/game_public.h",
		
		zlib_public,
		
		libpng_public,
		
		xatlas_public,
		xatlas_sources,
		
		glew_public,
		glew_sources,
	}
	
	filter "system:windows"
		removefiles {
			"**/*_linux.*"
		}
	filter {}
	filter "system:linux"
		removefiles {
			"**/*_win.*"
		}
	filter {}
	
	removefiles {
		"engine/client/cd_win.*",
		"engine/res/rw_*",
		"**/cd_vorbis.cpp",
		"**.def",
		"**/*sv_null.*",
		"**_pch.cpp"
	}
	
project "cgame_q2"
	kind "SharedLib"
	filter( filter_32bit )
		targetname "cgamex86"
	filter {}
	filter( filter_64bit )
		targetname "cgamex64"
	filter {}
	language "C++"
	targetdir "../game/baseq2"
		
	pchsource( "cgame_q2/cg_pch.cpp" )
	pchheader( "cg_local.h" )
	filter( "files:not cgame_q2/**" )
		flags( { "NoPCH" } )
	filter( {} )

	files {
		"common/*",
		"cgame_q2/*",
		"cgame_shared/*",
		
		"game_shared/m_flash.cpp"
	}
	
	filter "system:windows"
		removefiles {
			"**/*_linux.*"
		}
	filter {}
	filter "system:linux"
		removefiles {
			"**/*_win.*"
		}
	filter {}
	
	removefiles {
		"**.manifest",
	}
	
project "game_q2"
	kind "SharedLib"
	filter( filter_32bit )
		targetname "gamex86"
	filter {}
	filter( filter_64bit )
		targetname "gamex64"
	filter {}
	language "C++"
	targetdir "../game/baseq2"
	
	disablewarnings { "4244", "4311", "4302" }
	
	pchsource( "game_q2/g_pch.cpp" )
	pchheader( "g_local.h" )
	filter( "files:not game_q2/**" )
		flags( { "NoPCH" } )
	filter( {} )

	files {
		"common/*",
		"game_q2/*",
		"game_shared/*"
	}
	
	filter "system:windows"
		removefiles {
			"**/*_linux.*"
		}
	filter {}
	filter "system:linux"
		removefiles {
			"**/*_win.*"
		}
	filter {}
	
	removefiles {
		"**.manifest",
	}
	
-- Utils

filter "system:windows"

group "utilities"

project "qbsp4"
	kind "ConsoleApp"
	targetname "qbsp4"
	language "C++"
	floatingpoint "Default"
	targetdir "../game"
	includedirs { "utils/common2", "common" }
	
	files {
		"common/windows_default.manifest",
		
		"common/*.cpp",
		"common/*.h",
		
		"utils/common2/cmdlib.*",
		"utils/common2/mathlib.*",
		"utils/common2/scriplib.*",
		"utils/common2/polylib.*",
		"utils/common2/threads.*",
		"utils/common2/bspfile.*",
			
		"utils/qbsp4/*"
	}
	
	filter "system:windows"
		removefiles {
			"**/*_linux.*"
		}
	filter {}
	filter "system:linux"
		removefiles {
			"**/*_win.*"
		}
	filter {}

	
project "qvis4"
	kind "ConsoleApp"
	targetname "qvis4"
	language "C++"
	floatingpoint "Default"
	targetdir "../game"
	includedirs { "utils/common2", "common" }
	
	files {
		"common/windows_default.manifest",
		
		"common/*.cpp",
		"common/*.h",

		"utils/common2/cmdlib.*",
		"utils/common2/mathlib.*",
		"utils/common2/threads.*",
		"utils/common2/scriplib.*",
		"utils/common2/bspfile.*",
	
		"utils/qvis4/*"
	}
	
	filter "system:windows"
		removefiles {
			"**/*_linux.*"
		}
	filter {}
	filter "system:linux"
		removefiles {
			"**/*_win.*"
		}
	filter {}
	
project "qrad4"
	kind "ConsoleApp"
	targetname "qrad4"
	language "C++"
	floatingpoint "Default"
	targetdir "../game"
	includedirs { "utils/common2", "common", "thirdparty/stb" }
	
	files {
		"common/windows_default.manifest",
		
		"common/*.cpp",
		"common/*.h",
	
		"utils/common2/cmdlib.*",
		"utils/common2/mathlib.*",
		"utils/common2/threads.*",
		"utils/common2/polylib.*",
		"utils/common2/scriplib.*",
		"utils/common2/bspfile.*",
	
		"utils/qrad4/*"
	}
	
	filter "system:windows"
		removefiles {
			"**/*_linux.*"
		}
	filter {}
	filter "system:linux"
		removefiles {
			"**/*_win.*"
		}
	filter {}
	
project "qatlas"
	kind "ConsoleApp"
	targetname "qatlas"
	language "C++"
	floatingpoint "Default"
	targetdir "../game"
	includedirs { "utils/common2", "common", "thirdparty/xatlas" }
	
	files {
		"common/windows_default.manifest",
		"common/q_shared.*",
		"common/q_formats.h",
	
		"utils/common2/cmdlib.*",
		
		"utils/qatlas/*",
		
		xatlas_public,
		xatlas_sources
	}
	
--[[
project "light"
	kind "ConsoleApp"
	targetname "light"
	language "C"
	floatingpoint "Default"
	targetdir "../game"
	defines { "QE4" } -- We want the alternate VectorNormalize
	includedirs { "utils/common" }
	
	files {
		"common/windows_default.manifest",
	
		"utils/common/cmdlib.*",
		"utils/common/mathlib.*",
		"utils/common/scriplib.*",
		"utils/common/threads.*",
		"utils/common/bspfile.*",
	
		"utils/qrad3/trace.c",
		"utils/light/*"
	}
	
project "qdata"
	kind "ConsoleApp"
	targetname "qdata"
	language "C"
	floatingpoint "Default"
	targetdir "../game"
	includedirs { "utils/common", "thirdparty/stb" }
	
	files {
		"common/windows_default.manifest",
	
		"utils/common/cmdlib.*",
		"utils/common/scriplib.*",
		"utils/common/mathlib.*",
		"utils/common/trilib.*",
		"utils/common/lbmlib.*",
		"utils/common/threads.*",
		"utils/common/l3dslib.*",
		"utils/common/bspfile.*",
		"utils/common/md4.*",
	
		"utils/qdata/*"
	}

project "qe4"
	kind "WindowedApp"
	targetname "qe4"
	language "C"
	floatingpoint "Default"
	targetdir "../game"
	defines { "WIN_ERROR", "QE4" }
	includedirs { "utils/common", "thirdparty/stb" }
	links { "opengl32", "glu32" }
	
	files {
		"common/*.manifest",
		
		"utils/common/cmdlib.*",
		"utils/common/mathlib.*",
		"utils/common/bspfile.h",
		"utils/common/lbmlib.*",
		"utils/common/qfiles.*",
		
		"utils/qe4/*"
	}

filter {}
--]]

filter {}

-- Thirdparty

group "thirdparty"

project "zlib"
	kind "StaticLib"
	targetname "zlib"
	language "C"
	defines { "_CRT_NONSTDC_NO_WARNINGS" }
	
	disablewarnings { "4267" }
	
	files {
		zlib_public,
		zlib_sources
	}
	
project "libpng"
	kind "StaticLib"
	targetname "libpng"
	language "C"
	includedirs { "thirdparty/libpng_config", "thirdparty/zlib" }
	
	files {
		libpng_public,
		libpng_sources,
		
		"thirdparty/libpng_config/pnglibconf.h"
	}


project "uvatlas"
	kind "StaticLib"
	targetname "uvatlas"
	language "C++"
	includedirs { "thirdparty/UVAtlas/UVAtlas", "thirdparty/UVAtlas/UVAtlas/inc", "thirdparty/UVAtlas/UVAtlas/geodesics" }
	
	disablewarnings { "4530" }
	
	files {
		uvatlas_public,
		uvatlas_sources
	}
