cmake_minimum_required (VERSION 3.8)

project (VkEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENGINE_PROJ_NAME ${PROJECT_NAME})


if (NOT WIN32)
    message(FATAL_ERROR "Currently, only Windows is supported")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}. Use Debug/RelWithDebInfo (Profile)/Release")
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "${ENGINE_PROJ_NAME}: BUILD TYPE: DEBUG")
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        message(STATUS "${ENGINE_PROJ_NAME}: BUILD TYPE: PROFILE")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "${ENGINE_PROJ_NAME}: BUILD TYPE: RELEASE")
    endif()
endif()


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


FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.6
)
FetchContent_MakeAvailable(tinygltf)

target_compile_options(tinygltf PRIVATE -w)


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


# FetchContent_Declare(
#     vma
#     GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
#     GIT_TAG        v3.3.0
# )
# FetchContent_MakeAvailable(vma)

# add_library(vma INTERFACE)
# target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)

# # VMA generates a lot of warnings, so disable it
# target_compile_options(vma INTERFACE -w)


FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG        v0.12.2
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)


set(ENGINE_CXX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/source)

file(GLOB_RECURSE ENGINE_CXX_SOURCE_FILES
    ${ENGINE_CXX_SOURCE_DIR}/*.h
    ${ENGINE_CXX_SOURCE_DIR}/*.hpp
    ${ENGINE_CXX_SOURCE_DIR}/*.cpp
)

add_executable(${ENGINE_PROJ_NAME} ${ENGINE_CXX_SOURCE_FILES})


add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/shaders)
add_dependencies(${ENGINE_PROJ_NAME} ${SHADERS_PROJ_NAME})


target_precompile_headers(${ENGINE_PROJ_NAME}
    PRIVATE ${ENGINE_CXX_SOURCE_DIR}/pch.h
)

target_link_libraries(${ENGINE_PROJ_NAME}
    PRIVATE Vulkan::Vulkan
    PRIVATE glm 
    PRIVATE imgui
    PRIVATE tinygltf
    PRIVATE TracyClient

    PRIVATE
    $<$<PLATFORM_ID:Windows>:user32.lib gdi32.lib kernel32.lib>
)

target_include_directories(${ENGINE_PROJ_NAME} 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${ENGINE_CXX_SOURCE_DIR}
)

target_compile_options(${ENGINE_PROJ_NAME}
    PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct>
    PRIVATE $<$<CXX_COMPILER_ID:MSVC>: /W4 /WX>
)

target_compile_definitions(${ENGINE_PROJ_NAME}
    PRIVATE ENG_GFX_API_VULKAN

    PRIVATE $<$<CONFIG:Debug>:-DENG_BUILD_DEBUG>
    PRIVATE $<$<CONFIG:RelWithDebInfo>:-DENG_BUILD_PROFILE>
    PRIVATE $<$<CONFIG:Release>:-DENG_BUILD_RELEASE>

    PRIVATE $<$<PLATFORM_ID:Windows>:-DENG_OS_WINDOWS>
)

if (SHADER_USE_REVERSED_Z)
    target_compile_definitions(${ENGINE_PROJ_NAME}
        PRIVATE ENG_REVERSED_Z
    )
endif()