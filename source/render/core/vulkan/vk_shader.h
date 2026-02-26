#pragma once

#include "vk_device.h"

#include <span>
#include <array>


namespace vkn
{
    class Shader : public Object
    {        
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Shader);

        Shader() = default;
        Shader(Device* pDevice, VkShaderStageFlagBits stage, std::span<const uint8_t> spirv, std::string_view entryName = "main");

        ~Shader();

        Shader(Shader&& shader) noexcept;
        Shader& operator=(Shader&& shader) noexcept;

        Shader& Create(Device* pDevice, VkShaderStageFlagBits stage, std::span<const uint8_t> spirv, std::string_view entryName = "main");
        Shader& Destroy();

        template <typename... Args>
        Shader& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_module, VK_OBJECT_TYPE_SHADER_MODULE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const
        {
            return Object::GetDebugName("Shader");
        }

        Device& GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return *m_pDevice;
        }

        const VkShaderModule& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_module;
        }

        const VkShaderStageFlagBits& GetStage() const
        {
            VK_ASSERT(IsCreated());
            return m_stage;
        }

        const std::string_view GetEntryName() const
        {
            VK_ASSERT(IsCreated());
            return std::string_view(m_entryName.data(), m_entryName.size() - 1);
        }

        bool IsVertexShader() const
        {
            VK_ASSERT(IsCreated());
            return (m_stage & VK_SHADER_STAGE_VERTEX_BIT) != 0;
        }

        bool IsPixelShader() const
        {
            VK_ASSERT(IsCreated());
            return (m_stage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
        }

        bool IsComputeShader() const
        {
            VK_ASSERT(IsCreated());
            return (m_stage & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
        }

    private:
        Device* m_pDevice = nullptr;

        VkShaderModule m_module = VK_NULL_HANDLE;
        VkShaderStageFlagBits m_stage = {};

        std::array<char, 64> m_entryName = {};
    };
}