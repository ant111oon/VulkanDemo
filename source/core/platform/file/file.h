#pragma once

#include <vector>
#include <filesystem>


enum class FileOpenMode { BINARY, TEXT };

bool ReadFile(std::vector<uint8_t>& buffer, const std::filesystem::path& filepath, FileOpenMode mode = FileOpenMode::BINARY);
