#pragma once

#include <filesystem>


#define RND_SHADER_SPIRV_FULL_PATH(FILENAME) (std::filesystem::path(SHADERS_SPIRV_DIR "/") / std::filesystem::path(FILENAME)) 