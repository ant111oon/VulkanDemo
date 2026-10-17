cmake_minimum_required (VERSION 3.8)

project (VkEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENGINE_PROJ_NAME ${PROJECT_NAME})


message(STATUS "${ENGINE_PROJ_NAME}: C++ compiler: ${CMAKE_CXX_COMPILER}")

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


get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

# Single-config: ON unless Release
if(NOT IS_MULTI_CONFIG)
    if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        set(ENG_ENABLE_TRACY ON)
        set(ENG_ENABLE_DBG_DRAW ON)
    else()
        set(ENG_ENABLE_TRACY OFF)
        set(ENG_ENABLE_DBG_DRAW OFF)
    endif()
else()
    set(ENG_ENABLE_TRACY ON)
    set(ENG_ENABLE_DBG_DRAW ON)
endif()

# Optional: put it in cache so it's visible in CMake GUI / ccmake
set(ENG_ENABLE_TRACY ${ENG_ENABLE_TRACY} CACHE BOOL "Enable Tracy (non-Release only)" FORCE)
set(ENG_ENABLE_DBG_DRAW ${ENG_ENABLE_DBG_DRAW} CACHE BOOL "Enable Debug Draw" FORCE)


find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(glm)


FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        v0.9.0
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(fastgltf)


FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(stb)


FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        master
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(meshoptimizer)


if (ENG_ENABLE_TRACY)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.14.1
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(tracy)
endif()


if (ENG_ENABLE_DBG_DRAW)
    set(IMGUI_BUNDLE_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
    set(IMGUI_BUNDLE_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
    set(IMGUI_BUNDLE_WITH_HELLO_IMGUI OFF CACHE BOOL "" FORCE)
    set(IMGUI_BUNDLE_WITH_IMMAPP OFF CACHE BOOL "" FORCE)
    set(HELLOIMGUI_WITH_TEST_ENGINE OFF CACHE BOOL "" FORCE)

    # Enable the extensions you want
    set(IMGUI_BUNDLE_WITH_IMPLOT ON CACHE BOOL "" FORCE)
    set(IMGUI_BUNDLE_WITH_IMPLOT3D ON CACHE BOOL "" FORCE)
    set(IMGUI_BUNDLE_WITH_IMGUIZMO ON CACHE BOOL "" FORCE)
    # set(IMGUI_BUNDLE_WITH_IMGUI_NODE_EDITOR ON CACHE BOOL "" FORCE)

    # Avoid unnecessary graphics backend dependencies
    set(IMGUI_BUNDLE_WITH_NANOVG OFF CACHE BOOL "" FORCE)
    set(IMGUI_BUNDLE_WITH_IMMVISION OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        imgui_bundle
        GIT_REPOSITORY https://github.com/pthom/imgui_bundle.git
        GIT_TAG main
        GIT_PROGRESS TRUE
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(imgui_bundle)

    # Dear ImGui directory inside ImGui Bundle
    set(ENG_IMGUI_DIR "${imgui_bundle_SOURCE_DIR}/external/imgui/imgui")

    # Build backends separately
    add_library(imgui_backends STATIC
        "${ENG_IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        "${ENG_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    )

    target_include_directories(imgui_backends PUBLIC
        "${ENG_IMGUI_DIR}/backends"
    )

    target_link_libraries(imgui_backends PUBLIC
        imgui_bundle::imgui_bundle
        Vulkan::Vulkan
    )
endif()


FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.4.0
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(vma)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)

# VMA generates a lot of warnings, so disable it
target_compile_options(vma INTERFACE -w)


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
    PRIVATE vma
    PRIVATE glm
    PRIVATE fastgltf
    PRIVATE meshoptimizer
    
    PRIVATE $<$<NOT:$<CONFIG:Release>>:TracyClient imgui imgui_bundle::imgui_bundle imgui_backends>

    PRIVATE $<$<PLATFORM_ID:Windows>:user32.lib gdi32.lib kernel32.lib>
)

target_include_directories(${ENGINE_PROJ_NAME} 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${ENGINE_CXX_SOURCE_DIR}
    PRIVATE ${stb_SOURCE_DIR}
)

target_compile_options(${ENGINE_PROJ_NAME}
    PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -Wpedantic -Wno-gnu-anonymous-struct>
    PRIVATE $<$<CXX_COMPILER_ID:MSVC>: /W4 /WX>
)

target_compile_definitions(${ENGINE_PROJ_NAME}
    PRIVATE ENG_GFX_API_VULKAN
    PRIVATE SHADERS_SPIRV_DIR="${SHADERS_OUTPUT_BIN_DIR}"

    PRIVATE $<$<CONFIG:Debug>:-DENG_BUILD_DEBUG>
    PRIVATE $<$<CONFIG:RelWithDebInfo>:-DENG_BUILD_PROFILE>
    PRIVATE $<$<CONFIG:Release>:-DENG_BUILD_RELEASE>
)

if (WIN32)
    target_compile_definitions(${ENGINE_PROJ_NAME} PRIVATE -DENG_OS_WINDOWS)
else()
    message(FATAL_ERROR "Currently, only Windows is supported")
endif()

if (SHADER_USE_REVERSED_Z)
    target_compile_definitions(${ENGINE_PROJ_NAME}
        PRIVATE ENG_REVERSED_Z
    )
endif()