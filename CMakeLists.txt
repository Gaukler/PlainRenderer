cmake_minimum_required(VERSION 3.16)
#3.16 required for pre-compiled-headers

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Development" CACHE STRING "My" FORCE)

Project(Plain)

#require c++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE RUNTIME_FILES
    ${CMAKE_SOURCE_DIR}/Plain/src/Runtime/*.c
    ${CMAKE_SOURCE_DIR}/Plain/src/Runtime/*.cpp
    ${CMAKE_SOURCE_DIR}/Plain/src/Runtime/*.h
    ${CMAKE_SOURCE_DIR}/Plain/src/Runtime/*.hpp
    ${CMAKE_SOURCE_DIR}/resources/shaders/*.inc
    ${CMAKE_SOURCE_DIR}/resources/shaders/*.comp
    ${CMAKE_SOURCE_DIR}/resources/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/resources/shaders/*.geom
    ${CMAKE_SOURCE_DIR}/resources/shaders/*.frag)

file(GLOB_RECURSE ASSET_PIPELINE_FILES
    ${CMAKE_SOURCE_DIR}/Plain/src/AssetPipeline/*.c
    ${CMAKE_SOURCE_DIR}/Plain/src/AssetPipeline/*.cpp
    ${CMAKE_SOURCE_DIR}/Plain/src/AssetPipeline/*.h
    ${CMAKE_SOURCE_DIR}/Plain/src/AssetPipeline/*.hpp)

file(GLOB_RECURSE COMMON_FILES
    ${CMAKE_SOURCE_DIR}/Plain/src/Common/*.c
    ${CMAKE_SOURCE_DIR}/Plain/src/Common/*.cpp
    ${CMAKE_SOURCE_DIR}/Plain/src/Common/*.h
    ${CMAKE_SOURCE_DIR}/Plain/src/Common/*.hpp)

add_executable(PlainRuntime
    ${RUNTIME_FILES}
    ${COMMON_FILES})

#asset pipeline executable
add_executable(PlainAssetPipeline
    ${ASSET_PIPELINE_FILES}
    ${COMMON_FILES})

#add src/ as include to avoid relative include paths
include_directories(Plain/src)
include_directories(Plain/src/Common)

#add vendor folder to allow direct inclusion of header libraries
include_directories(Plain/vendor)

#add glfw
add_subdirectory(Plain/vendor/glfw)
target_link_libraries(PlainRuntime glfw ${GLFW_LIBRARIES})

#add SPIRV-Cross
add_subdirectory(Plain/vendor/spirv-cross)
target_link_libraries(PlainRuntime spirv-cross-c)

#add glm
add_subdirectory(Plain/vendor/glm)
include_directories(Plain/vendor/glm)
#glm is header only, no target_link_libraries needed

#add glslang
add_subdirectory(Plain/vendor/glslang)
target_link_libraries(PlainRuntime glslang SPIRV)

#add imgui
include_directories(Plain/vendor/imgui)
set(IMGUI_FILES 
    Plain/vendor/imgui/imgui.h
    Plain/vendor/imgui/imgui_internal.h
    Plain/vendor/imgui/imstb_rectpack.h
    Plain/vendor/imgui/imstb_textedit.h
    Plain/vendor/imgui/imstb_truetype.h
    Plain/vendor/imgui/backends/imgui_impl_vulkan.h
    Plain/vendor/imgui/backends/imgui_impl_glfw.h

    Plain/vendor/imgui/imgui.cpp
    Plain/vendor/imgui/imgui_demo.cpp
    Plain/vendor/imgui/imgui_draw.cpp
    Plain/vendor/imgui/imgui_widgets.cpp
    Plain/vendor/imgui/imgui_tables.cpp
    Plain/vendor/imgui/backends/imgui_impl_vulkan.cpp
    Plain/vendor/imgui/backends/imgui_impl_glfw.cpp)
target_sources(PlainRuntime PRIVATE ${IMGUI_FILES})
source_group("ImGui" FILES ${IMGUI_FILES})

#add tinygltf
include_directories(Plain/vendor/tinygltf)
set(TINY_GLTF_FILES 
    Plain/vendor/tinygltf/tiny_gltf.h
    Plain/vendor/tinygltf/stb_image.h
    Plain/vendor/tinygltf/stb_image_write.h
    Plain/vendor/tinygltf/json.hpp)
target_sources(PlainAssetPipeline PRIVATE ${TINY_GLTF_FILES})
source_group("tinygltf" FILES ${TINY_GLTF_FILES})

#add vulkan
find_package(Vulkan REQUIRED)
target_include_directories(PlainRuntime PRIVATE Vulkan::Vulkan)
target_link_libraries(PlainRuntime Vulkan::Vulkan)

#configure precompiled headers
target_precompile_headers(PlainRuntime 	        PRIVATE ${CMAKE_SOURCE_DIR}/Plain/src/Common/pch.h)
target_precompile_headers(PlainAssetPipeline    PRIVATE ${CMAKE_SOURCE_DIR}/Plain/src/Common/pch.h)

#set source groups to create proper filters in visual studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${RUNTIME_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${UTILITIES_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ASSET_PIPELINE_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${COMMON_FILES})

add_library(CommonCompileOptions INTERFACE)

#configure VS configurations
if(MSVC)

    #enable multi core compilation and set warning level 4
    target_compile_options(CommonCompileOptions INTERFACE "/MP" "/W4")

    #whole program optimization for release build
    target_compile_options(CommonCompileOptions INTERFACE $<$<CONFIG:Release>:/GL>) 

    #set development build debug settings
    target_compile_options(CommonCompileOptions INTERFACE $<$<CONFIG:Development>:/Zi>) 
    target_link_options(CommonCompileOptions    INTERFACE $<$<CONFIG:Development>:/DEBUG>)
endif()

target_link_libraries(PlainRuntime          CommonCompileOptions)
target_link_libraries(PlainAssetPipeline    CommonCompileOptions)

#runtime macros per config
target_compile_definitions(PlainRuntime PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:Development>>:USE_VK_VALIDATION_LAYERS>)