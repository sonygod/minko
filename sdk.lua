if os.getenv('MINKO_HOME') then
	MINKO_HOME = os.getenv('MINKO_HOME');
else
	print(color.fg.red .. 'You must define the environment variable MINKO_HOME.' .. color.reset)
	os.exit(1)
end

if not os.isfile(MINKO_HOME .. '/sdk.lua') then
	print(color.fg.red ..'MINKO_HOME does not point to a valid Minko SDK.' .. color.reset)
	os.exit(1)
end

print('Minko SDK home directory: ' .. MINKO_HOME)

require 'emscripten'
require 'android'

local insert = require 'insert'

insert.insert(premake.tools.gcc, 'cxxflags.system', {
	linux = { "-MMD", "-MP", "-std=c++11" },
	macosx = { "-MMD", "-MP", "-std=c++11" },
	emscripten = { "-MMD", "-MP", "-std=c++11" }
})

insert.insert(premake.tools.clang, 'cxxflags.system', {
	macosx = { "-MMD", "-MP", "-std=c++11", "-stdlib=libc++" }
})

insert.insert(premake.tools.clang, 'ldflags.system.macosx', {
	macosx = { "-stdlib=libc++" }
})

-- print(table.inspect(premake.tools.clang))

-- distributable SDK
MINKO_SDK_DIST = true

-- import build system utilities
dofile(MINKO_HOME .. '/tools/all/lib/minko.lua')
dofile(MINKO_HOME .. '/tools/all/lib/minko.sdk.lua')
dofile(MINKO_HOME .. '/tools/all/lib/minko.os.lua')
dofile(MINKO_HOME .. '/tools/all/lib/minko.path.lua')
dofile(MINKO_HOME .. '/tools/all/lib/minko.plugin.lua')
dofile(MINKO_HOME .. '/tools/all/lib/minko.vs.lua')
dofile(MINKO_HOME .. '/tools/all/lib/minko.project.lua')


-- options
if _OPTIONS.platform then
	print('Selected target platform: ' .. _OPTIONS['platform'])
	-- overwrite the native platform with the options::platform
	premake.gcc.platforms['Native'] = premake.gcc.platforms[_OPTIONS.platform]
end

newoption {
	trigger	= "no-stencil",
	description = "Disable all stencil operations."
}
if _OPTIONS["no-stencil"] then
	defines { "MINKO_NO_STENCIL" }
end
