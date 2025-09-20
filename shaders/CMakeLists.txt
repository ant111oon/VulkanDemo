cmake_minimum_required (VERSION 3.8)

project (SHADERS)

set(SHADERS_PROJ_NAME ${PROJECT_NAME})
set(SHADERS_PROJ_NAME ${SHADERS_PROJ_NAME} PARENT_SCOPE)


set(SHADERS_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/source)
set(SHADERS_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SHADERS_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}. Use Debug/RelWithDebInfo (Profile)/Release")
endif()

if (NOT EXISTS ${SHADERS_OUTPUT_DIR})
    message(STATUS "${SHADERS_PROJ_NAME}: Creating ${SHADERS_OUTPUT_DIR}...")

    make_directory(${SHADERS_OUTPUT_DIR})
endif()


set(SHADER_COMPILER $ENV{VULKAN_SDK}/Bin/slangc.exe)
set(SHADER_TARGET spirv)
set(SHADER_ENTRY_NAME main)
set(SHADER_PROFILE spirv_1_4)

set(SHADER_COMPILER_COMMON_FLAGS
    -emit-spirv-directly
    -fvk-use-entrypoint-name
    -I${SHADERS_INCLUDE_DIR}
    -target ${SHADER_TARGET}
    -entry ${SHADER_ENTRY_NAME}
    -profile ${SHADER_PROFILE}
)

set(SHADER_COMPILER_FLAGS
    ${SHADER_COMPILER_COMMON_FLAGS}

    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-DDEBUG>

    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O>
)

message(STATUS "${SHADERS_PROJ_NAME}: Compiler flags: ${SHADER_COMPILER_FLAGS}")


file(GLOB_RECURSE SHADER_SRC_FILES
    ${SHADERS_SRC_DIR}/*.slang
)

foreach(SHADER_SRC_FILE ${SHADER_SRC_FILES})
    get_filename_component(FILE_NAME ${SHADER_SRC_FILE} NAME_WLE)
    
    set(SHADER_SPIRV_FILE ${SHADERS_OUTPUT_DIR}/${FILE_NAME}.spv)
    
    add_custom_command(
        OUTPUT ${SHADER_SPIRV_FILE}
        COMMAND ${SHADER_COMPILER} ${SHADER_SRC_FILE} ${SHADER_COMPILER_FLAGS} -o ${SHADER_SPIRV_FILE}
        DEPENDS ${SHADER_SRC_FILE}
        COMMENT "Compiling ${SHADER_SRC_FILE} -> ${SHADER_SPIRV_FILE}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SHADER_SPIRV_FILE})
endforeach(SHADER_SRC_FILE)

add_custom_target(${SHADERS_PROJ_NAME} DEPENDS ${SPIRV_BINARY_FILES})