#include "pch.h"

#include "vk_pipeline.h"


namespace vkn
{
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


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetVertexShader(VkShaderModule shader, const char* pEntryName)
    {
        return SetShaderInfo(SHADER_STAGE_VERTEX, shader, pEntryName);
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPixelShader(VkShaderModule shader, const char* pEntryName)
    {
        return SetShaderInfo(SHADER_STAGE_PIXEL, shader, pEntryName);
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


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRenderingDepthAttachmentFormat(VkFormat format)
    {
        m_renderingCreateInfo.depthAttachmentFormat = format;
        return *this;
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRenderingStencilAttachmentFormat(VkFormat format)
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


    VkPipeline GraphicsPipelineBuilder::Build(VkDevice vkDevice)
    {   
    #if defined(ENG_BUILD_DEBUG)
        for (size_t i = 0; i < m_shaderStages.size(); ++i) {
            CORE_ASSERT_MSG(m_shaderStages[i].module != VK_NULL_HANDLE, "Shader stage (index: %zu) module is VK_NULL_HANDLE", i);
        }
    #endif

        CORE_ASSERT_MSG(m_colorBlendAttachmentStatesCount == m_colorAttachmentFormatsCount, "Color attachments count and color blend states count must be equal");
        CORE_ASSERT_MSG(m_layout != VK_NULL_HANDLE, "Graphics pipeline layout is not set");
        CORE_ASSERT_MSG(
            m_colorAttachmentFormatsCount > 0 || 
            m_renderingCreateInfo.depthAttachmentFormat != VK_FORMAT_UNDEFINED || 
            m_renderingCreateInfo.stencilAttachmentFormat != VK_FORMAT_UNDEFINED,
            "There is no format set for any of the graphics pipeline attachments"
        );

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
        VK_CHECK(vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &vkPipeline));
        VK_ASSERT(vkPipeline != VK_NULL_HANDLE);

        return vkPipeline;
    }

    
    constexpr VkShaderStageFlagBits GraphicsPipelineBuilder::ShaderStageIndexToFlagBits(ShaderStageIndex index)
    {
        switch(index) {
            case SHADER_STAGE_VERTEX:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case SHADER_STAGE_PIXEL:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            default:
                CORE_ASSERT_FAIL("Invalid shader stage index");
                return {};
        }
    }


    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetShaderInfo(ShaderStageIndex index, VkShaderModule shader, const char* pEntryName)
    {   
        CORE_ASSERT(pEntryName && strlen(pEntryName) <= MAX_SHADER_ENTRY_NAME_LENGTH);
        auto& entryName = m_shaderEntryNames[index];
        strcpy_s(entryName.data(), MAX_SHADER_ENTRY_NAME_LENGTH, pEntryName);

        VkPipelineShaderStageCreateInfo& shaderStageCreateInfo = m_shaderStages[index];

        shaderStageCreateInfo.module = shader;
        shaderStageCreateInfo.pName = entryName.data();
        shaderStageCreateInfo.stage = ShaderStageIndexToFlagBits(index);

        return *this;
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


    VkPipeline ComputePipelineBuilder::Build(VkDevice vkDevice)
    {
        CORE_ASSERT(m_createInfo.layout);
        CORE_ASSERT(m_createInfo.stage.module);

        VkPipeline vkPipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(vkDevice, VK_NULL_HANDLE, 1, &m_createInfo, nullptr, &vkPipeline));
        VK_ASSERT(vkPipeline != VK_NULL_HANDLE);

        return vkPipeline;
    }


    PipelineLayoutBuilder::PipelineLayoutBuilder(size_t maxPushConstBlockSize)
    {
        Reset();
        SetMaxPushConstBlockSize(maxPushConstBlockSize);
    }


    PipelineLayoutBuilder& PipelineLayoutBuilder::Reset()
    {
        m_pushConstRanges.fill({});
        m_pushConstRangeCount = 0;

        m_layouts.fill(VK_NULL_HANDLE);
        m_layoutCount = 0;

        m_flags = {};
        m_maxPushConstBlockSize = 0;

        return *this;
    }


    PipelineLayoutBuilder& PipelineLayoutBuilder::SetMaxPushConstBlockSize(size_t size)
    {
        m_maxPushConstBlockSize = size;
        return *this;
    }


    PipelineLayoutBuilder& PipelineLayoutBuilder::SetFlags(VkPipelineLayoutCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }


    PipelineLayoutBuilder& PipelineLayoutBuilder::AddPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size)
    {
        VK_ASSERT(m_pushConstRangeCount + 1 <= MAX_PUSH_CONSTANT_RANGE_COUNT);
        VK_ASSERT(offset + size <= m_maxPushConstBlockSize);
        
        m_pushConstRanges[m_pushConstRangeCount].stageFlags = stages;
        m_pushConstRanges[m_pushConstRangeCount].offset = offset;
        m_pushConstRanges[m_pushConstRangeCount].size = size;

        ++m_pushConstRangeCount;

        return *this;
    }


    PipelineLayoutBuilder& PipelineLayoutBuilder::AddDescriptorSetLayout(VkDescriptorSetLayout vkSetLayout)
    {
        VK_ASSERT(vkSetLayout != VK_NULL_HANDLE);
        VK_ASSERT(m_layoutCount + 1 <= MAX_DESCRIPTOR_SET_LAYOUT_COUNT);
        
        m_layouts[m_layoutCount] = vkSetLayout;

        ++m_layoutCount;

        return *this;
    }


    VkPipelineLayout PipelineLayoutBuilder::Build(VkDevice vkDevice)
    {
        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.flags = m_flags;
        createInfo.setLayoutCount = m_layoutCount;
        createInfo.pSetLayouts = m_layouts.data();
        createInfo.pushConstantRangeCount = m_pushConstRangeCount;
        createInfo.pPushConstantRanges = m_pushConstRanges.data();

        VkPipelineLayout vkLayout = VK_NULL_HANDLE;
        VK_CHECK(vkCreatePipelineLayout(vkDevice, &createInfo, nullptr, &vkLayout));
        VK_ASSERT(vkLayout != VK_NULL_HANDLE);

        return vkLayout;
    }


    DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(size_t bindingsCount)
    {
        Reset();
        m_bindings.reserve(bindingsCount);
    }


    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::Reset()
    {
        m_bindings.clear();
        m_flags = 0;

        return *this;
    }


    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stages)
    {
        VK_ASSERT_MSG(!IsBindingExist(binding), "Binding %u has already been added", binding);

        VkDescriptorSetLayoutBinding descriptor = {};
        descriptor.binding = binding;
        descriptor.descriptorType = type;
        descriptor.descriptorCount = descriptorCount;
        descriptor.stageFlags = stages;

        m_bindings.emplace_back(descriptor);

        return *this;
    }


    VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VkDevice vkDevice)
    {
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.flags = m_flags;
        descriptorSetLayoutCreateInfo.bindingCount = m_bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = m_bindings.data();

        VkDescriptorSetLayout vkDescriptorSetLayout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(vkDevice, &descriptorSetLayoutCreateInfo, nullptr, &vkDescriptorSetLayout));
        VK_ASSERT(vkDescriptorSetLayout != VK_NULL_HANDLE);

        return vkDescriptorSetLayout;
    }

    
    bool DescriptorSetLayoutBuilder::IsBindingExist(uint32_t bindingNumber)
    {
        return std::find_if(m_bindings.cbegin(), m_bindings.cend(), [bindingNumber](const VkDescriptorSetLayoutBinding& binding){
            return binding.binding == bindingNumber;
        }) != m_bindings.cend();
    }


    DescriptorPoolBuilder::DescriptorPoolBuilder(size_t resourcesTypesCount)
    {
        Reset();
        m_poolSizes.reserve(resourcesTypesCount);
    }


    DescriptorPoolBuilder& DescriptorPoolBuilder::Reset()
    {
        m_poolSizes.clear();
        m_maxDescriptorSets = 0;
        m_flags = 0;

        return *this;
    }


    DescriptorPoolBuilder& DescriptorPoolBuilder::SetFlags(VkDescriptorPoolCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }


    DescriptorPoolBuilder& DescriptorPoolBuilder::SetMaxDescriptorSetsCount(size_t count)
    {
        m_maxDescriptorSets = count;
        return *this;
    }


    DescriptorPoolBuilder& DescriptorPoolBuilder::AddResource(VkDescriptorType type, uint32_t descriptorCount)
    {
        VkDescriptorPoolSize descPoolSize = {};
        descPoolSize.type = type;
        descPoolSize.descriptorCount = descriptorCount;

        m_poolSizes.emplace_back(descPoolSize);

        return *this;
    }


    VkDescriptorPool DescriptorPoolBuilder::Build(VkDevice vkDevice)
    {
        VkDescriptorPoolCreateInfo descPoolCreateInfo = {};
        descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolCreateInfo.flags = m_flags;
        descPoolCreateInfo.maxSets = m_maxDescriptorSets;
        descPoolCreateInfo.poolSizeCount = m_poolSizes.size();
        descPoolCreateInfo.pPoolSizes = m_poolSizes.data();

        VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(vkDevice, &descPoolCreateInfo, nullptr, &vkDescriptorPool));
        VK_ASSERT(vkDescriptorPool != VK_NULL_HANDLE);

        return vkDescriptorPool;
    }


    DescriptorSetAllocator::DescriptorSetAllocator(uint32_t layoutsCount)
    {
        Reset();
        m_layouts.reserve(layoutsCount);
    }
    

    DescriptorSetAllocator& DescriptorSetAllocator::Reset()
    {
        m_layouts.clear();
        m_vkDescPool = VK_NULL_HANDLE;

        return *this;
    }


    DescriptorSetAllocator& DescriptorSetAllocator::SetPool(VkDescriptorPool vkPool)
    {
        VK_ASSERT(vkPool != VK_NULL_HANDLE);
        m_vkDescPool = vkPool;
        
        return *this;
    }


    DescriptorSetAllocator& DescriptorSetAllocator::AddLayout(VkDescriptorSetLayout vkLayout)
    {
        VK_ASSERT(vkLayout != VK_NULL_HANDLE);
        m_layouts.emplace_back(vkLayout);
        
        return *this;
    }


    void DescriptorSetAllocator::Allocate(VkDevice vkDevice, std::span<VkDescriptorSet> outDescriptorSets)
    {
        VK_ASSERT(outDescriptorSets.size() >= m_layouts.size());

        VkDescriptorSetAllocateInfo descSetAllocInfo = {};
        descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetAllocInfo.descriptorPool = m_vkDescPool;
        descSetAllocInfo.descriptorSetCount = m_layouts.size();
        descSetAllocInfo.pSetLayouts = m_layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(vkDevice, &descSetAllocInfo, outDescriptorSets.data()));
    }
}