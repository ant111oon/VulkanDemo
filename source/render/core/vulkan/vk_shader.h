#pragma once

#include "vk_device.h"

#include <span>
#include <array>


namespace vkn
{
    class Shader : public Handle<VkShaderModule>
    {
    public:
        using Base = Handle<VkShaderModule>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Shader);

        Shader() = default;
        Shader(Device* pDevice, VkShaderStageFlagBits stage, std::span<const uint8_t> spirv, std::string_view entryName = "main");

        ~Shader();

        Shader(Shader&& shader) noexcept;
        Shader& operator=(Shader&& shader) noexcept;

        Shader& Create(Device* pDevice, VkShaderStageFlagBits stage, std::span<const uint8_t> spirv, std::string_view entryName = "main");
        Shader& Destroy();

        Device& GetDevice() const;

        const VkShaderStageFlagBits& GetStage() const;

        const std::string_view GetEntryName() const;

        bool IsVertexShader() const;
        bool IsPixelShader() const;
        bool IsComputeShader() const;

    private:
        Device* m_pDevice = nullptr;

        VkShaderStageFlagBits m_stage = {};

        std::array<char, 64> m_entryName = {};
    };
}