#include "pch.h"

#include "vk_shader.h"


namespace vkn
{
    Shader::Shader(Device* pDevice, VkShaderStageFlagBits stage, std::span<const uint8_t> spirv, std::string_view entryName)
    {
        Create(pDevice, stage, spirv, entryName);
    }


    Shader::~Shader()
    {
        Destroy();
    }
    

    Shader::Shader(Shader&& shader) noexcept
    {
        *this = std::move(shader);
    }


    Shader& Shader::operator=(Shader&& shader) noexcept
    {
        if (this == &shader) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, shader.m_pDevice);

        std::swap(m_module, shader.m_module);
        std::swap(m_stage, shader.m_stage);
        std::swap(m_entryName, shader.m_entryName);

        Object::operator=(std::move(shader));

        return *this; 
    }


    Shader& Shader::Create(Device* pDevice, VkShaderStageFlagBits stage, std::span<const uint8_t> spirv, std::string_view entryName)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of shader %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(pDevice && pDevice->IsCreated());
        VK_ASSERT(!spirv.empty());
        VK_ASSERT_MSG(spirv.size() % sizeof(uint32_t) == 0, "SPIR-V code must be alligned by uint32_t size");

        VK_ASSERT_MSG(
            (stage & VK_SHADER_STAGE_VERTEX_BIT) != 0 ||
            (stage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0 ||
            (stage & VK_SHADER_STAGE_COMPUTE_BIT) != 0,
            "Unsupported shader stage"
        );

        VkShaderModuleCreateInfo shaderCreateInfo = {};
        shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
        shaderCreateInfo.codeSize = spirv.size();

        VK_CHECK(vkCreateShaderModule(pDevice->Get(), &shaderCreateInfo, nullptr, &m_module));
        VK_ASSERT(m_module != VK_NULL_HANDLE);

        m_pDevice = pDevice;
        m_stage = stage;

        memcpy(m_entryName.data(), entryName.data(), std::min(m_entryName.size() - 1, entryName.size()));

        SetCreated(true);

        return *this;
    }


    Shader& Shader::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }
        
        vkDestroyShaderModule(m_pDevice->Get(), m_module, nullptr);
        m_module = VK_NULL_HANDLE;
        
        m_stage = {};
        m_entryName.fill(0);
        
        m_pDevice = nullptr;

        Object::Destroy();

        return *this;
    }
}