#include "pch.h"

#include "file.h"


bool ReadFile(std::vector<uint8_t>& buffer, const std::filesystem::path& filepath, FileOpenMode mode)
{
    auto openMode = std::ios::ate;
    openMode |= (mode == FileOpenMode::BINARY ? std::ios::binary : 0);

    std::ifstream file(filepath, openMode);

    if (!file.is_open()) {
        return false;
    }

    const size_t fileSize = (size_t)file.tellg();

    buffer.resize(fileSize);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return true;
}