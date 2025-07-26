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

target_include_directories(engine 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${ENGINE_CXX_SOURCE_DIR}
)

target_link_libraries(engine PRIVATE Vulkan::Vulkan vma glm imgui)
if(WIN32)
    target_link_libraries(engine PRIVATE 
        user32.lib gdi32.lib kernel32.lib
    )
endif()

target_precompile_headers(engine PRIVATE ${ENGINE_CXX_SOURCE_DIR}/pch.h)

target_compile_options(engine PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>: /W4 /WX>
)

target_compile_definitions(engine PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)


add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/shaders)
add_dependencies(engine ${SHADERS_TARGET_NAME})