#include "pch.h"

#include "vk_pso.h"
#include "vk_device.h"
#include "vk_descriptor.h"


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
        std::swap(m_layout, layout.m_layout);

        Object::operator=(std::move(layout));

        return *this;
    }


    PSOLayout& PSOLayout::Create(const PSOLayoutCreateInfo& info)
    {
        return Create(info.pDevice, info.setLayouts, info.pushConstantRanges, info.flags);
    }


    PSOLayout& PSOLayout::Create(Device* pDevice, std::span<const DescriptorSetLayout*> setLayouts, std::span<const VkPushConstantRange> pushConstantRanges, VkPipelineLayoutCreateFlags flags)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of PSO layout %s", GetDebugName());
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
        const VkDeviceSize maxPushConstantsSize = pDevice->GetPhysDevice()->GetProperties().properties.limits.maxPushConstantsSize;

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

        VK_CHECK(vkCreatePipelineLayout(pDevice->Get(), &createInfo, nullptr, &m_layout));
        VK_ASSERT(m_layout != VK_NULL_HANDLE);

        m_pDevice = pDevice;

        SetCreated(true);

        return *this;
    }


    PSOLayout& PSOLayout::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vkDestroyPipelineLayout(m_pDevice->Get(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();

        return *this;
    }


























    GraphicsPipelineBuilder& GraphicsPipelineBuilder::Reset()
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

        m_layout = VK_NULL_HANDLE;

        m_flags = {};

        for (VkPipelineShaderStageCreateInfo& shaderStage : m_shaderStages) {
            shaderStage = {};
            shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        }

        for (auto& name : m_shaderEntryNames) {
            name.fill('\0');
        }

        std::fill(m_dynamicStateValues.begin(), m_dynamicStateValues.end(), VK_DYNAMIC_STATE_MAX_ENUM);
        m_dynamicStatesCount = 0;

        m_viewportsAndScissorCount = 0;
        m_colorAttachmentFormatsCount = 0;
        m_colorBlendAttachmentStatesCount = 0;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetFlags(VkPipelineCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetLayout(VkPipelineLayout layout)
    {
        m_layout = layout;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddShader(VkShaderModule shader, VkShaderStageFlagBits stage, const char* pEntryName)
    {
        CORE_ASSERT(pEntryName && strlen(pEntryName) <= MAX_SHADER_ENTRY_NAME_LENGTH);
        for (const VkPipelineShaderStageCreateInfo& shaderStage : m_shaderStages) {
            CORE_ASSERT_MSG(shaderStage.stage != stage, "There already is shader with same shader stage: %d", stage);
        }
        
        const size_t index = m_shaderStages.size();

        m_shaderStages.resize(index + 1);
        m_shaderEntryNames.resize(m_shaderStages.size());

        auto& entryName = m_shaderEntryNames[index];
        strcpy_s(entryName.data(), MAX_SHADER_ENTRY_NAME_LENGTH, pEntryName);

        VkPipelineShaderStageCreateInfo& shaderStageCreateInfo = m_shaderStages[index];
        shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfo.module = shader;
        shaderStageCreateInfo.stage = stage;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
    {
        m_inputAssemblyState.topology = topology;
        m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddDynamicState(VkDynamicState state)
    {
        CORE_ASSERT(m_dynamicStatesCount + 1 <= MAX_DYNAMIC_STATES_COUNT);
        m_dynamicStateValues[m_dynamicStatesCount++] = state;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddDynamicState(const std::span<const VkDynamicState> states)
    {
        for (VkDynamicState state : states) {
            AddDynamicState(state);
        }

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddViewportAndScissor(const VkViewport& viewport, const VkRect2D& scissor)
    {
        CORE_ASSERT(m_viewportsAndScissorCount + 1 <= MAX_VIEWPORT_AND_SCISSOR_COUNT);

        m_viewports[m_viewportsAndScissorCount] = viewport;
        m_scissors[m_viewportsAndScissorCount] = scissor;
        
        ++m_viewportsAndScissorCount;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerLineWidth(float lineWidth)
    {
        m_rasterizationState.lineWidth = lineWidth;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerDepthClampEnabled(VkBool32 enabled)
    {
        m_rasterizationState.depthClampEnable = enabled;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerDiscardEnabled(VkBool32 enabled)
    {
        m_rasterizationState.rasterizerDiscardEnable = enabled;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerPolygonMode(VkPolygonMode polygonMode)
    {
        m_rasterizationState.polygonMode = polygonMode;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerCullMode(VkCullModeFlags cullMode)
    {
        m_rasterizationState.cullMode = cullMode;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerFrontFace(VkFrontFace frontFace)
    {
        m_rasterizationState.frontFace = frontFace;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerDepthBias(VkBool32 enabled, float biasConstantFactor, float biasClamp, float biasSlopeFactor)
    {
        m_rasterizationState.depthBiasEnable = enabled;
        m_rasterizationState.depthBiasConstantFactor = biasConstantFactor;
        m_rasterizationState.depthBiasClamp = biasClamp;
        m_rasterizationState.depthBiasSlopeFactor = biasSlopeFactor;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthTestState(VkBool32 testEnabled, VkBool32 depthWriteEnable, VkCompareOp compareOp)
    {
        m_depthStencilState.depthTestEnable = testEnabled;
        m_depthStencilState.depthWriteEnable = depthWriteEnable;
        m_depthStencilState.depthCompareOp = compareOp;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetStencilTestState(VkBool32 testEnabled, const VkStencilOpState& front, const VkStencilOpState& back)
    {
        m_depthStencilState.stencilTestEnable = testEnabled;
        m_depthStencilState.front = front;
        m_depthStencilState.back = back;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthBoundsTestState(VkBool32 depthBoundsTestEnable, float minValue, float maxValue)
    {
        m_depthStencilState.depthBoundsTestEnable = depthBoundsTestEnable;
        m_depthStencilState.minDepthBounds = minValue;
        m_depthStencilState.maxDepthBounds = maxValue;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRenderingViewMask(uint32_t viewMask)
    {
        m_renderingCreateInfo.viewMask = viewMask;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthAttachmentFormat(VkFormat format)
    {
        m_renderingCreateInfo.depthAttachmentFormat = format;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetStencilAttachmentFormat(VkFormat format)
    {
        m_renderingCreateInfo.stencilAttachmentFormat = format;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddColorAttachmentFormat(VkFormat format)
    {
        CORE_ASSERT(m_colorAttachmentFormatsCount + 1 <= MAX_COLOR_ATTACHMENTS_COUNT);
        m_colorAttachmentFormats[m_colorAttachmentFormatsCount++] = format;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddColorAttachmentFormat(const std::span<const VkFormat> formats)
    {
        for (VkFormat format : formats) {
            AddColorAttachmentFormat(format);
        }
        
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetColorBlendConstants(float r, float g, float b, float a)
    {
        m_colorBlendState.blendConstants[0] = r;
        m_colorBlendState.blendConstants[1] = g;
        m_colorBlendState.blendConstants[2] = b;
        m_colorBlendState.blendConstants[3] = a;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetColorBlendLogicOp(VkBool32 logicOpEnable, VkLogicOp logicOp)
    {
        m_colorBlendState.logicOpEnable = logicOpEnable;
        m_colorBlendState.logicOp = logicOp;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddColorBlendAttachment(
        VkBool32 blendEnable, 
        VkBlendFactor srcColorBlendFactor, 
        VkBlendFactor dstColorBlendFactor,
        VkBlendOp colorBlendOp,
        VkBlendFactor srcAlphaBlendFactor,
        VkBlendFactor dstAlphaBlendFactor,
        VkBlendOp alphaBlendOp,
        VkColorComponentFlags colorWriteMask
    ) {
        CORE_ASSERT(m_colorBlendAttachmentStatesCount + 1 <= MAX_COLOR_ATTACHMENTS_COUNT);

        VkPipelineColorBlendAttachmentState& attachment = m_colorBlendAttachmentStates[m_colorBlendAttachmentStatesCount];
        attachment.blendEnable = blendEnable;
        attachment.srcColorBlendFactor = srcColorBlendFactor;
        attachment.dstColorBlendFactor = dstColorBlendFactor;
        attachment.colorBlendOp = colorBlendOp;
        attachment.srcAlphaBlendFactor = srcAlphaBlendFactor;
        attachment.dstAlphaBlendFactor = dstAlphaBlendFactor;
        attachment.alphaBlendOp = alphaBlendOp;
        attachment.colorWriteMask = colorWriteMask;

        ++m_colorBlendAttachmentStatesCount;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddColorBlendAttachment(const VkPipelineColorBlendAttachmentState& blendState)
    {
        CORE_ASSERT(m_colorBlendAttachmentStatesCount + 1 <= MAX_COLOR_ATTACHMENTS_COUNT);
        m_colorBlendAttachmentStates[m_colorBlendAttachmentStatesCount++] = blendState;

        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddColorBlendAttachment(const std::span<const VkPipelineColorBlendAttachmentState> blendStates)
    {
        for (const VkPipelineColorBlendAttachmentState& blendState : blendStates) {
            AddColorBlendAttachment(blendState);
        }

        return *this;
    }


    VkPipeline GraphicsPipelineBuilder::Build()
    {
        CORE_ASSERT_MSG(m_colorBlendAttachmentStatesCount == m_colorAttachmentFormatsCount, "Color attachments count and color blend states count must be equal");
        CORE_ASSERT_MSG(m_layout != VK_NULL_HANDLE, "Graphics pipeline layout is not set");
        CORE_ASSERT_MSG(
            m_colorAttachmentFormatsCount > 0 || 
            m_renderingCreateInfo.depthAttachmentFormat != VK_FORMAT_UNDEFINED || 
            m_renderingCreateInfo.stencilAttachmentFormat != VK_FORMAT_UNDEFINED,
            "There is no format set for any of the graphics pipeline attachments"
        );

        for (size_t i = 0; i < m_shaderStages.size(); ++i) {
            CORE_ASSERT_MSG(m_shaderStages[i].module != VK_NULL_HANDLE, "Shader stage (index: %zu) module is VK_NULL_HANDLE", i);
            m_shaderStages[i].pName = m_shaderEntryNames[i].data();
        }

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        m_renderingCreateInfo.colorAttachmentCount = m_colorAttachmentFormatsCount;
        m_renderingCreateInfo.pColorAttachmentFormats = m_colorAttachmentFormats.data();
        pipelineCreateInfo.pNext = &m_renderingCreateInfo;
        
        pipelineCreateInfo.stageCount = m_shaderStages.size();
        pipelineCreateInfo.pStages = m_shaderStages.data();

        pipelineCreateInfo.flags = m_flags;

        pipelineCreateInfo.pVertexInputState = &m_vertexInputState;
        
        pipelineCreateInfo.pInputAssemblyState = &m_inputAssemblyState;

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        if (m_viewportsAndScissorCount == 0) {
            viewportStateCreateInfo.viewportCount = 1;
            viewportStateCreateInfo.scissorCount = 1;
        } else {
            viewportStateCreateInfo.viewportCount = m_viewportsAndScissorCount;
            viewportStateCreateInfo.pViewports = m_viewports.data();
            viewportStateCreateInfo.scissorCount = m_viewportsAndScissorCount;
            viewportStateCreateInfo.pScissors = m_scissors.data();
        }
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        
        pipelineCreateInfo.pRasterizationState = &m_rasterizationState;

        pipelineCreateInfo.pMultisampleState = &m_multisampleState;

        pipelineCreateInfo.pDepthStencilState = &m_depthStencilState;
        
        m_colorBlendState.attachmentCount = m_colorBlendAttachmentStatesCount;
        m_colorBlendState.pAttachments = m_colorBlendAttachmentStates.data();
        pipelineCreateInfo.pColorBlendState = &m_colorBlendState;
        
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = m_dynamicStatesCount;
        dynamicState.pDynamicStates = m_dynamicStateValues.data();
        
        pipelineCreateInfo.pDynamicState = &dynamicState;

        pipelineCreateInfo.layout = m_layout;

        VkPipeline vkPipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateGraphicsPipelines(GetDevice().Get(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &vkPipeline));
        VK_ASSERT(vkPipeline != VK_NULL_HANDLE);

        return vkPipeline;
    }


    ComputePipelineBuilder& ComputePipelineBuilder::Reset()
    {
        m_createInfo = {};
        m_createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        m_createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        m_shaderEntryName.fill('\0');

        return *this;
    }


    ComputePipelineBuilder& ComputePipelineBuilder::SetFlags(VkPipelineCreateFlags flags)
    {
        m_createInfo.flags = flags;
        return *this;
    }


    ComputePipelineBuilder& ComputePipelineBuilder::SetLayout(VkPipelineLayout layout)
    {
        m_createInfo.layout = layout;
        return *this;
    }


    ComputePipelineBuilder& ComputePipelineBuilder::SetShader(VkShaderModule shader, const char* pEntryName)
    {
        CORE_ASSERT(pEntryName && strlen(pEntryName) <= MAX_SHADER_ENTRY_NAME_LENGTH);
        strcpy_s(m_shaderEntryName.data(), MAX_SHADER_ENTRY_NAME_LENGTH, pEntryName);

        m_createInfo.stage.module = shader;
        m_createInfo.stage.pName = m_shaderEntryName.data();
        m_createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

        return *this;
    }


    VkPipeline ComputePipelineBuilder::Build()
    {
        CORE_ASSERT(m_createInfo.layout);
        CORE_ASSERT(m_createInfo.stage.module);

        VkPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(GetDevice().Get(), VK_NULL_HANDLE, 1, &m_createInfo, nullptr, &pipeline));
        VK_ASSERT(pipeline != VK_NULL_HANDLE);

        return pipeline;
    }
}