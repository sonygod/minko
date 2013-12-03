newoption {
	trigger			= "with-sdl",
	description		= "Enable the Minko SDL plugin."
}

minko.project.library "plugin-sdl"
	kind "StaticLib"
	language "C++"
	files { "**.hpp", "**.h", "**.cpp", "**.c" }
	includedirs { "include", "src"	}
	
	minko.plugin.import("angle")
	
	configuration { "debug"}
		defines { "DEBUG" }
		flags { "Symbols" }
		targetdir "bin/debug"

	configuration { "release" }
		defines { "NDEBUG" }
		flags { "OptimizeSpeed" }
		targetdir "bin/release"

	-- linux
	configuration { "linux" }
		includedirs { "lib/SDL2/include" }

	-- windows
	configuration { "windows" }
		includedirs { "lib/SDL2/include" }
		
	-- macos
	configuration { "macosx" }
		includedirs { "lib/SDL2/include" }

	-- emscripten
	configuration { "emscripten" }
		defines { "EMSCRIPTEN" }
		flags { "Optimize" }
		includedirs { "lib/emscripten/SDL/include" }
