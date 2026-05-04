#include "pch.h"

#include "vk_pso.h"
#include "vk_device.h"
#include "vk_descriptor.h"
#include "vk_shader.h"


namespace vkn
{
    PSOLayout::PSOLayout(const PSOLayoutCreateInfo& info)
    {
        Create(info);
    }


    PSOLayout::PSOLayout(Device* pDevice, std::span<const DescriptorSetLayout*> setLayouts, std::span<const VkPushConstantRange> pushConstantRanges, VkPipelineLayoutCreateFlags flags)
    {
        Create(pDevice, setLayouts, pushConstantRanges, flags);
    }

    
    PSOLayout::~PSOLayout()
    {
        Destroy();
    }
    

    PSOLayout::PSOLayout(PSOLayout&& layout) noexcept
    {
        *this = std::move(layout);
    }


    PSOLayout& PSOLayout::operator=(PSOLayout&& layout) noexcept
    {
        if (this == &layout) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, layout.m_pDevice);

        Base::operator=(std::move(layout));

        return *this;
    }


    PSOLayout& PSOLayout::Create(const PSOLayoutCreateInfo& info)
    {
        return Create(info.pDevice, info.setLayouts, info.pushConstantRanges, info.flags);
    }


    PSOLayout& PSOLayout::Create(Device* pDevice, std::span<const DescriptorSetLayout*> setLayouts, std::span<const VkPushConstantRange> pushConstantRanges, VkPipelineLayoutCreateFlags flags)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of PSO layout %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(pDevice && pDevice->IsCreated());
        VK_ASSERT(!setLayouts.empty());

        std::vector<VkDescriptorSetLayout> descSetLayouts(setLayouts.size());
        
        for (size_t i = 0; i < descSetLayouts.size(); ++i) {
            VK_ASSERT(setLayouts[i] != nullptr && setLayouts[i]->IsCreated());
            descSetLayouts[i] = setLayouts[i]->Get();
        }

    #ifdef ENG_BUILD_DEBUG
        const VkDeviceSize maxPushConstantsSize = pDevice->GetPhysDevice().GetProperties().properties.limits.maxPushConstantsSize;

        for (const VkPushConstantRange& range : pushConstantRanges) {
            VK_ASSERT_MSG(range.offset + range.size <= maxPushConstantsSize, 
                "Out of push constant range, offset: %zu, size: %zu, max size: %zu", range.offset, range.size, maxPushConstantsSize);
        }
    #endif

        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.flags = flags;
        createInfo.setLayoutCount = descSetLayouts.size();
        createInfo.pSetLayouts = descSetLayouts.data();
        createInfo.pushConstantRangeCount = pushConstantRanges.size();
        createInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();

        Base::Create([pDevice, &createInfo](VkPipelineLayout& layout) {
            VK_CHECK(vkCreatePipelineLayout(pDevice->Get(), &createInfo, nullptr, &layout));
            return layout != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

        m_pDevice = pDevice;

        return *this;
    }


    PSOLayout& PSOLayout::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Base::Destroy([pDevice = m_pDevice](VkPipelineLayout& layout) {
            vkDestroyPipelineLayout(pDevice->Get(), layout, nullptr);
        });

        m_pDevice = nullptr;

        return *this;
    }


    Device& PSOLayout::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }


    PSO::PSO(PSOLayout* pLayout, VkPipeline pso, State state)
    {
        Create(pLayout, pso, state);
    }


    PSO& PSO::Create(PSOLayout* pLayout, VkPipeline pso, State state)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of PSO %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(pLayout && pLayout->IsCreated());
        
        Base::Create([&pso](VkPipeline& dstPSO) {
            dstPSO = pso;
            return dstPSO != VK_NULL_HANDLE;
        });

        m_pLayout = pLayout;
        m_state = state;

        return *this;
    }


    PSO::~PSO()
    {
        Destroy();
    }


    PSO::PSO(PSO&& pso) noexcept
    {
        *this = std::move(pso);
    }


    PSO& PSO::operator=(PSO&& pso) noexcept
    {
        if (this == &pso) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pLayout, pso.m_pLayout);
        std::swap(m_state, pso.m_state);

        Base::operator=(std::move(pso));

        return *this; 
    }


    PSO& PSO::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Base::Destroy([vkDevice = GetDevice().Get()](VkPipeline& pso) {
            vkDestroyPipeline(vkDevice, pso, nullptr);
        });

        m_pLayout = nullptr;
        m_state.reset();

        return *this;
    }


    VkPipelineBindPoint PSO::GetBindPoint() const
    {
        VK_ASSERT(IsCreated());
        
        if (IsRasterization()) {
            return VK_PIPELINE_BIND_POINT_GRAPHICS;
        } else if (IsCompute()) {
            return VK_PIPELINE_BIND_POINT_COMPUTE;
        }

        VK_ASSERT_FAIL("Unknown PSO type");
        return VK_PIPELINE_BIND_POINT_MAX_ENUM;
    }


    Device& PSO::GetDevice() const
    {
        return GetLayout().GetDevice();
    }


    PSOLayout& PSO::GetLayout() const
    {
        VK_ASSERT(IsCreated());
        return *m_pLayout;
    }


    bool PSO::IsRasterization() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_RASTERIZATION_PSO);
    }


    bool PSO::IsCompute() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_COMPUTE_PSO);
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::Reset()
    {
        m_vertexInputState = {};
        m_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        m_inputAssemblyState = {};
        m_inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        m_rasterizationState = {};
        m_rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_rasterizationState.lineWidth = 1.f;
        m_rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        m_rasterizationState.cullMode = VK_CULL_MODE_NONE;

        m_multisampleState = {};
        m_multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        m_depthStencilState = {};
        m_depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        m_colorBlendState = {};
        m_colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        m_renderingCreateInfo = {};
        m_renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        m_pLayout = nullptr;

        m_flags = {};

        m_shaderStages.clear();
        m_viewports.clear();
        m_scissors.clear();
        m_colorAttachmentFormats.clear();
        m_dynamicStateValues.clear();
        m_shaderStages.clear();
        m_colorBlendAttachmentStates.clear();

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetFlags(VkPipelineCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetLayout(PSOLayout& layout)
    {
        m_pLayout = &layout;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::AddShader(vkn::Shader& shader)
    {
        VK_ASSERT(shader.IsCreated());
        VK_ASSERT(shader.IsVertexShader() || shader.IsPixelShader());

        VkPipelineShaderStageCreateInfo& shaderStageCreateInfo = m_shaderStages.emplace_back();
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.module = shader.Get();
        shaderStageCreateInfo.stage = shader.GetStage();
        shaderStageCreateInfo.pName = shader.GetEntryName().data();

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
    {
        m_inputAssemblyState.topology = topology;
        m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::AddDynamicState(VkDynamicState state)
    {
        m_dynamicStateValues.emplace_back(state);
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::AddDynamicState(const std::span<const VkDynamicState> states)
    {
        for (VkDynamicState state : states) {
            AddDynamicState(state);
        }

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::AddViewportAndScissor(const VkViewport& viewport, const VkRect2D& scissor)
    {
        m_viewports.emplace_back(viewport);
        m_scissors.emplace_back(scissor);

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerLineWidth(float lineWidth)
    {
        m_rasterizationState.lineWidth = lineWidth;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerDepthClampState(VkBool32 depthClampEnable)
    {
        m_rasterizationState.depthClampEnable = depthClampEnable;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerDiscardState(VkBool32 rasterizerDiscardEnable)
    {
        m_rasterizationState.rasterizerDiscardEnable = rasterizerDiscardEnable;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerPolygonMode(VkPolygonMode polygonMode)
    {
        m_rasterizationState.polygonMode = polygonMode;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerCullMode(VkCullModeFlags cullMode)
    {
        m_rasterizationState.cullMode = cullMode;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerFrontFace(VkFrontFace frontFace)
    {
        m_rasterizationState.frontFace = frontFace;
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRasterizerDepthBiasState(VkBool32 depthBiasEnable, float biasConstantFactor, float biasClamp, float biasSlopeFactor)
    {
        m_rasterizationState.depthBiasEnable = depthBiasEnable;
        m_rasterizationState.depthBiasConstantFactor = biasConstantFactor;
        m_rasterizationState.depthBiasClamp = biasClamp;
        m_rasterizationState.depthBiasSlopeFactor = biasSlopeFactor;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetDepthTestState(VkBool32 enabled, VkCompareOp compareOp)
    {
        m_depthStencilState.depthTestEnable = enabled;
        m_depthStencilState.depthCompareOp = compareOp;
        
        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetDepthWriteState(VkBool32 enabled)
    {
        m_depthStencilState.depthWriteEnable = enabled;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetStencilTestState(VkBool32 enabled, const VkStencilOpState& front, const VkStencilOpState& back)
    {
        m_depthStencilState.stencilTestEnable = enabled;
        m_depthStencilState.front = front;
        m_depthStencilState.back = back;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetDepthBoundsTestState(VkBool32 enabled, float minValue, float maxValue)
    {
        m_depthStencilState.depthBoundsTestEnable = enabled;
        m_depthStencilState.minDepthBounds = minValue;
        m_depthStencilState.maxDepthBounds = maxValue;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetRenderingViewMask(uint32_t viewMask)
    {
        m_renderingCreateInfo.viewMask = viewMask;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetColorBlendConstants(float r, float g, float b, float a)
    {
        m_colorBlendState.blendConstants[0] = r;
        m_colorBlendState.blendConstants[1] = g;
        m_colorBlendState.blendConstants[2] = b;
        m_colorBlendState.blendConstants[3] = a;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetColorBlendLogicOpState(VkBool32 enabled, VkLogicOp logicOp)
    {
        m_colorBlendState.logicOpEnable = enabled;
        m_colorBlendState.logicOp = logicOp;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetDepthAttachment(VkFormat format)
    {
        m_renderingCreateInfo.depthAttachmentFormat = format;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::SetStencilAttachmentFormat(VkFormat format)
    {
        m_renderingCreateInfo.stencilAttachmentFormat = format;

        return *this;
    }


    GraphicsPSOBuilder& GraphicsPSOBuilder::AddColorAttachment(
        VkFormat format,
        VkColorComponentFlags colorWriteMask, 
        VkBool32 blendEnable,
        VkBlendFactor srcColorBlendFactor, 
        VkBlendFactor dstColorBlendFactor,
        VkBlendOp colorBlendOp,
        VkBlendFactor srcAlphaBlendFactor,
        VkBlendFactor dstAlphaBlendFactor,
        VkBlendOp alphaBlendOp            
    ) {
        VkPipelineColorBlendAttachmentState& attachment = m_colorBlendAttachmentStates.emplace_back();
        attachment.blendEnable = blendEnable;
        attachment.srcColorBlendFactor = srcColorBlendFactor;
        attachment.dstColorBlendFactor = dstColorBlendFactor;
        attachment.colorBlendOp = colorBlendOp;
        attachment.srcAlphaBlendFactor = srcAlphaBlendFactor;
        attachment.dstAlphaBlendFactor = dstAlphaBlendFactor;
        attachment.alphaBlendOp = alphaBlendOp;
        attachment.colorWriteMask = colorWriteMask;

        m_colorAttachmentFormats.emplace_back(format);

        return *this;
    }


    PSO GraphicsPSOBuilder::Build()
    {
        CORE_ASSERT_MSG(m_pLayout && m_pLayout->IsCreated(), "Graphics PSO layout is invalid");
        CORE_ASSERT_MSG(
            !m_colorAttachmentFormats.empty() || 
            m_renderingCreateInfo.depthAttachmentFormat != VK_FORMAT_UNDEFINED || 
            m_renderingCreateInfo.stencilAttachmentFormat != VK_FORMAT_UNDEFINED,
            "There is no format set for any of the graphics PSO attachments"
        );

        VkGraphicsPipelineCreateInfo psoCreateInfo = {};
        psoCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        m_renderingCreateInfo.colorAttachmentCount = m_colorAttachmentFormats.size();
        m_renderingCreateInfo.pColorAttachmentFormats = m_colorAttachmentFormats.empty() ? nullptr : m_colorAttachmentFormats.data();
        psoCreateInfo.pNext = &m_renderingCreateInfo;
        
        psoCreateInfo.stageCount = m_shaderStages.size();
        psoCreateInfo.pStages = m_shaderStages.data();

        psoCreateInfo.flags = m_flags;

        psoCreateInfo.pVertexInputState = &m_vertexInputState;
        
        psoCreateInfo.pInputAssemblyState = &m_inputAssemblyState;

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        
        if (m_viewports.empty()) {
            viewportStateCreateInfo.viewportCount = 1;
        } else {
            viewportStateCreateInfo.viewportCount = m_viewports.size();
            viewportStateCreateInfo.pViewports = m_viewports.data();
        }

        if (m_scissors.empty()) {
            viewportStateCreateInfo.scissorCount = 1;
        } else {
            viewportStateCreateInfo.scissorCount = m_scissors.size();
            viewportStateCreateInfo.pScissors = m_scissors.data();
        }

        psoCreateInfo.pViewportState = &viewportStateCreateInfo;
        
        psoCreateInfo.pRasterizationState = &m_rasterizationState;

        psoCreateInfo.pMultisampleState = &m_multisampleState;

        psoCreateInfo.pDepthStencilState = &m_depthStencilState;
        
        m_colorBlendState.attachmentCount = m_colorBlendAttachmentStates.size();
        m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.empty() ? nullptr : m_colorBlendAttachmentStates.data();
        psoCreateInfo.pColorBlendState = &m_colorBlendState;
        
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = m_dynamicStateValues.size();
        dynamicState.pDynamicStates = m_dynamicStateValues.empty() ? nullptr : m_dynamicStateValues.data();
        
        psoCreateInfo.pDynamicState = &dynamicState;

        psoCreateInfo.layout = m_pLayout->Get();

        VkPipeline pso = VK_NULL_HANDLE;
        VK_CHECK(vkCreateGraphicsPipelines(GetDevice().Get(), VK_NULL_HANDLE, 1, &psoCreateInfo, nullptr, &pso));
        VK_ASSERT(pso != VK_NULL_HANDLE);

        PSO::State state = {};
        state.set(PSO::StateBits::BIT_IS_RASTERIZATION_PSO);

        return PSO(m_pLayout, pso, state);
    }


    ComputePSOBuilder& ComputePSOBuilder::Reset()
    {
        m_createInfo = {};
        m_createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        m_createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        m_pLayout = nullptr;

        return *this;
    }


    ComputePSOBuilder& ComputePSOBuilder::SetFlags(VkPipelineCreateFlags flags)
    {
        m_createInfo.flags = flags;
        return *this;
    }


    ComputePSOBuilder& ComputePSOBuilder::SetLayout(PSOLayout& layout)
    {
        VK_ASSERT(layout.IsCreated());

        m_pLayout = &layout;
        m_createInfo.layout = m_pLayout->Get();

        return *this;
    }


    ComputePSOBuilder& ComputePSOBuilder::SetShader(Shader& shader)
    {
        VK_ASSERT(shader.IsCreated());
        VK_ASSERT(shader.IsComputeShader());

        m_createInfo.stage.module = shader.Get();
        m_createInfo.stage.pName = shader.GetEntryName().data();
        m_createInfo.stage.stage = shader.GetStage();

        return *this;
    }


    PSO ComputePSOBuilder::Build()
    {
        CORE_ASSERT(m_createInfo.stage.module != VK_NULL_HANDLE);
        CORE_ASSERT(m_pLayout && m_pLayout->IsCreated());

        VkPipeline pso = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(GetDevice().Get(), VK_NULL_HANDLE, 1, &m_createInfo, nullptr, &pso));
        VK_ASSERT(pso != VK_NULL_HANDLE);

        PSO::State state = {};
        state.set(PSO::StateBits::BIT_IS_COMPUTE_PSO);

        return PSO(m_pLayout, pso, state);
    }
}