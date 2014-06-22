-- TODO: this is turning into kind of a giant blob of complexity. Someday this should be cleaned up, but it's not yet clear what lines it should be cleaned up along.

dofile("src/premake/util.lua")

newoption {
  trigger = "ue",
  value = "version",
  description = "Choose an Unreal Engine version to target; requires vs2013 target",
  allowed = {
    { "4_2", "Unreal Engine 4.2" },
  }
}

-- utility functions
function filereplace(filename, src, dst)
  local f = io.open(filename, "rb")
  local data = f:read("*all")
  f:close()
  
  data = data:gsub(src, dst)
  
  f = io.open(filename, "wb")
  f:write(data)
  f:close()
end
-- end utility functions

local projectInfo = {}

if _ACTION == "vs2008" then
  projectInfo.slug = "msvc9"
  projectInfo.platform = "win"
  projectInfo.platformFull = "win_msvc9_"
elseif _ACTION == "vs2010" then
  projectInfo.slug = "msvc10"
  projectInfo.platform = "win"
  projectInfo.platformFull = "win_msvc10_"
elseif _ACTION == "vs2012" then
  projectInfo.slug = "msvc11"
  projectInfo.platform = "win"
  projectInfo.platformFull = "win_msvc11_"
elseif _ACTION == "vs2013" and not _OPTIONS["ue"] then
  projectInfo.slug = "msvc12"
  projectInfo.platform = "win"
  projectInfo.platformFull = "win_msvc12_"
elseif _ACTION == "vs2013" and _OPTIONS["ue"] == "4_2" then
  projectInfo.slug = "ue4_2"
  projectInfo.platform = "win"
  projectInfo.platformFull = "win_msvc12_"
  projectInfo.ue4_path = "C:/Program Files/Unreal Engine/4.2/"
  os.execute([["/Program Files/Unreal Engine/Launcher/Engine/Binaries/Win64/UnrealVersionSelector.exe" /projectfiles %cd%/ue4/ue4.uproject]])
  filereplace("ue4/Intermediate/ProjectFiles/ue4.vcxproj", "$%(SolutionDir%)$%(SolutionName%)", "$(ProjectDir)/../../$(ProjectName)")
else
  print(("Not supported: target %s with OS %s"):format(_ACTION or "", _OS or ""))
  projectInfo.slug = ""
  projectInfo.platform = ""
  projectInfo.platformFull = ""
end

projectInfo.path = "projects/" .. projectInfo.slug

solution "Frames"
  configurations { "Debug", "Release" }
  if not projectInfo.ue4_path then
    platforms { "x32", "x64" }
  else
    platforms { "x64" } -- currently editor only, which means x64 only
    defines "_ITERATOR_DEBUG_LEVEL=0" -- ue4 doesn't like iterator debug levels, and that means neither do we
  end
  
  flags { "FatalWarnings" }
  
  -- Paths
  location(projectInfo.path)
  
  local function libincludes(suffix)
    if not projectInfo.ue4_path then
      includedirs {
        "include",
        "deps/boost_1_55_0",
        "deps/freetype-2.5.3/" .. projectInfo.platform .. suffix .. "/include",
        "deps/glew-1.10.0/" .. projectInfo.platform .. suffix .. "/include",
        "deps/jpeg-9/" .. projectInfo.platform .. suffix .. "/include",
        "deps/libpng-1.4.3/" .. projectInfo.platform .. suffix .. "/include",
        "deps/lua-5.1.5/" .. projectInfo.platform .. suffix .. "/include",
        "deps/SDL2-2.0.1/" .. projectInfo.platform .. suffix .. "/include",
        "deps/zlib-1.2.8/" .. projectInfo.platform .. suffix .. "/include",
        "deps/gtest-1.7.0/" .. projectInfo.platformFull .. suffix .. "/include",
      }
      libdirs {
        "deps/boost_1_55_0",
        "deps/freetype-2.5.3/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/glew-1.10.0/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/jpeg-9/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/libpng-1.4.3/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/lua-5.1.5/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/SDL2-2.0.1/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/zlib-1.2.8/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/gtest-1.7.0/" .. projectInfo.platformFull .. suffix .. "/lib",
      }
    else
      local ue4libsuffix = "Win" .. suffix .. "/VS2013"
      includedirs {
        "include",
        "deps/boost_1_55_0",
        projectInfo.ue4_path .. "/Engine/Source/ThirdParty/FreeType2/FreeType2-2.4.12/include",
        "deps/glew-1.10.0/" .. projectInfo.platform .. suffix .. "/include",
        "deps/jpeg-9/" .. projectInfo.platform .. suffix .. "/include",
        projectInfo.ue4_path .. "/Engine/Source/ThirdParty/libPNG/libPNG-1.5.2",
        "deps/lua-5.1.5/" .. projectInfo.platform .. suffix .. "/include",
        "deps/SDL2-2.0.1/" .. projectInfo.platform .. suffix .. "/include",
        projectInfo.ue4_path .. "/Engine/Source/ThirdParty/zlib/zlib-1.2.5/Inc",
        "deps/gtest-1.7.0/" .. projectInfo.platformFull .. suffix .. "/include",
      }
      libdirs {
        "deps/boost_1_55_0",
        projectInfo.ue4_path .. "/Engine/Source/ThirdParty/FreeType2/FreeType2-2.4.12/Lib/" .. ue4libsuffix,
        "deps/glew-1.10.0/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/jpeg-9/" .. projectInfo.platform .. suffix .. "/lib",
        projectInfo.ue4_path .. "/Engine/Source/ThirdParty/libPNG/libPNG-1.5.2/lib/" .. ue4libsuffix,
        "deps/lua-5.1.5/" .. projectInfo.platform .. suffix .. "/lib",
        "deps/SDL2-2.0.1/" .. projectInfo.platform .. suffix .. "/lib",
        projectInfo.ue4_path .. "/Engine/Source/ThirdParty/zlib/zlib-1.2.5/Lib/Win" .. suffix,
        "deps/gtest-1.7.0/" .. projectInfo.platformFull .. suffix .. "/lib",
      }
    end
  end
  
  if projectInfo.platform == "win" then
    defines "NOMINMAX"  -- disable min/max macros from windows headers
    defines "WIN32_LEAN_AND_MEAN"  -- make windows.h not quite as titanically huge
  end
  
  -- projectInfo.platform-specific tweaks
  configuration "x32"
    libincludes("32")
  
  configuration "x64"
    libincludes("64")
  
  -- Compiler-specific tweaks
  configuration "vs*"
    defines "_CRT_SECURE_NO_WARNINGS" -- Annoying warning on MSVC that wants use of MSVC-specific functions
    buildoptions "/wd4800"  -- "forcing value to bool 'true' or 'false' (performance warning)"
  
  configuration { "vs2008 or vs2010" }
    includedirs {
      "C:/Program Files (x86)/Windows Kits/8.1/Include/um",
      "C:/Program Files (x86)/Windows Kits/8.1/Include/shared"
    }
      
  configuration { "vs2008 or vs2010", "x32" }
    libdirs "C:/Program Files (x86)/Windows Kits/8.1/Lib/winv6.3/um/x86"
    
  configuration { "vs2008 or vs2010", "x64" }
    libdirs "C:/Program Files (x86)/Windows Kits/8.1/Lib/winv6.3/um/x64"
  
  -- Build config
  configuration "*"
    flags { "Symbols" } -- always create debug symbols
  
  configuration "Debug"
    targetsuffix "d"

  configuration "Release"
    optimize "Full"
  
  -- Frames
  project "frames"
    kind "StaticLib"
    language "C++"
    location(projectInfo.path)
    files "src/core/*.cpp"
    files "include/frames/*.h"
    
    configuration "x32"
      targetdir("lib/" .. projectInfo.slug .. "/x32")
        
    configuration "x64"
      targetdir("lib/" .. projectInfo.slug .. "/x64")
  
  -- OGL core
  project "frames_opengl"
    kind "StaticLib"
    language "C++"
    location(projectInfo.path)
    files "src/opengl/*.cpp"
    files "include/frames/renderer_opengl.h"
    
    configuration "x32"
      targetdir("lib/" .. projectInfo.slug .. "/x32")
        
    configuration "x64"
      targetdir("lib/" .. projectInfo.slug .. "/x64")
  
  -- DX11 core
  project "frames_dx11"
    kind "StaticLib"
    language "C++"
    location(projectInfo.path)
    files "src/dx11/*.cpp"
    files "include/frames/renderer_dx11.h"
    
    configuration "x32"
      targetdir("lib/" .. projectInfo.slug .. "/x32")
        
    configuration "x64"
      targetdir("lib/" .. projectInfo.slug .. "/x64")
  
  if projectInfo.ue4_path then
    external "ue4"
      location "ue4/Intermediate/ProjectFiles"
      kind "StaticLib"
      
      -- this will have to be fixed, but I don't yet grok the ue4 configuration options
      configmap {
        ["Debug"] = {"DebugGame_Editor", "Windows"},
        ["Release"] = {"Development_Editor", "Windows"},
      }

      links "frames"
  end
  
  -- Null core
  project "frames_null"
    kind "StaticLib"
    language "C++"
    location(projectInfo.path)
    files "src/null/*.cpp"
    files "include/frames/renderer_null.h"
    
    configuration "x32"
      targetdir("lib/" .. projectInfo.slug .. "/x32")
        
    configuration "x64"
      targetdir("lib/" .. projectInfo.slug .. "/x64")
      
    dofile("src/premake/project_test.lua", projectInfo)
