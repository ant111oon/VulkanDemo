cmake_minimum_required (VERSION 3.8)

project (VkEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9b
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(imgui PUBLIC Vulkan::Vulkan)


FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.3.0
)
FetchContent_MakeAvailable(vma)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)


set(ENGINE_CXX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/source)

file(GLOB_RECURSE CPP_SOURCE_FILES
    ${ENGINE_CXX_SOURCE_DIR}/*.h
    ${ENGINE_CXX_SOURCE_DIR}/*.hpp
    ${ENGINE_CXX_SOURCE_DIR}/*.cpp
)

add_executable(engine ${CPP_SOURCE_FILES})

target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_include_directories(engine 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${ENGINE_CXX_SOURCE_DIR}
)

target_link_libraries(engine 
    PUBLIC vma 
    PUBLIC glm 
    PUBLIC imgui 
    PUBLIC Vulkan::Vulkan
)

target_precompile_headers(engine PUBLIC ${ENGINE_CXX_SOURCE_DIR}/pch.h)

target_compile_options(engine PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>: /W4 /WX>)

# add_custom_command(TARGET engine POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
#     COMMAND_EXPAND_LISTS
# )

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(SHADER_COMPILER $ENV{VULKAN_SDK}/Bin/glslc.exe)
else()
    set(SHADER_COMPILER $ENV{VULKAN_SDK}/Bin32/glslc.exe)
endif()

set(ENGINE_SHADERS_DIR ${PROJECT_SOURCE_DIR}/shaders)
set(ENGINE_SHADERS_SRC_DIR ${ENGINE_SHADERS_DIR}/source)
set(ENGINE_SHADERS_BIN_DIR ${ENGINE_SHADERS_DIR}/bin)

file(GLOB_RECURSE SHADER_SRC_FILES
    ${ENGINE_SHADERS_SRC_DIR}/*.frag  
    ${ENGINE_SHADERS_SRC_DIR}/*.vert
    ${ENGINE_SHADERS_SRC_DIR}/*.comp
)

foreach(SHADER_SRC_FILE ${SHADER_SRC_FILES})
    get_filename_component(FILE_NAME ${SHADER_SRC_FILE} NAME)
    
    set(SHADER_SPIRV_FILE ${ENGINE_SHADERS_BIN_DIR}/${FILE_NAME}.spv)
    
    message(STATUS "${SHADER_SRC_FILE} will be compiled to ${SHADER_SPIRV_FILE}")
    
    add_custom_command(
        OUTPUT ${SHADER_SPIRV_FILE}
        COMMAND ${SHADER_COMPILER} ${SHADER_SRC_FILE} -o ${SHADER_SPIRV_FILE}
        DEPENDS ${SHADER_SRC_FILE})
    list(APPEND SPIRV_BINARY_FILES ${SHADER_SPIRV_FILE})
endforeach(SHADER_SRC_FILE)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(engine shaders)