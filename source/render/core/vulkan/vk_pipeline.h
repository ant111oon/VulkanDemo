#pragma once

#include "vk_core.h"


namespace vkn
{
    class GraphicsPipelineBuilder
    {
    public:
        GraphicsPipelineBuilder() { Reset(); }

        GraphicsPipelineBuilder& Reset();

        GraphicsPipelineBuilder& SetFlags(VkPipelineCreateFlags flags);

        GraphicsPipelineBuilder& SetLayout(VkPipelineLayout layout);

        GraphicsPipelineBuilder& SetVertexShader(VkShaderModule shader, const char* pEntryName = "main");
        GraphicsPipelineBuilder& SetPixelShader(VkShaderModule shader, const char* pEntryName = "main");

        GraphicsPipelineBuilder& SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);

        GraphicsPipelineBuilder& AddDynamicState(VkDynamicState state);

        GraphicsPipelineBuilder& AddDynamicState(const std::span<const VkDynamicState> states);

        GraphicsPipelineBuilder& AddViewportAndScissor(const VkViewport& viewport, const VkRect2D& scissor);

        GraphicsPipelineBuilder& SetRasterizerLineWidth(float lineWidth);

        GraphicsPipelineBuilder& SetRasterizerDepthClampEnabled(VkBool32 enabled);

        GraphicsPipelineBuilder& SetRasterizerDiscardEnabled(VkBool32 enabled);

        GraphicsPipelineBuilder& SetRasterizerPolygonMode(VkPolygonMode polygonMode);

        GraphicsPipelineBuilder& SetRasterizerCullMode(VkCullModeFlags cullMode);

        GraphicsPipelineBuilder& SetRasterizerFrontFace(VkFrontFace frontFace);

        GraphicsPipelineBuilder& SetRasterizerDepthBias(VkBool32 enabled, float biasConstantFactor, float biasClamp, float biasSlopeFactor);

        GraphicsPipelineBuilder& SetDepthTestState(VkBool32 testEnabled, VkBool32 depthWriteEnable, VkCompareOp compareOp);

        GraphicsPipelineBuilder& SetStencilTestState(VkBool32 testEnabled, const VkStencilOpState& front, const VkStencilOpState& back);

        GraphicsPipelineBuilder& SetDepthBoundsTestState(VkBool32 depthBoundsTestEnable, float minValue, float maxValue);

        GraphicsPipelineBuilder& SetRenderingViewMask(uint32_t viewMask);

        GraphicsPipelineBuilder& SetDepthAttachmentFormat(VkFormat format);

        GraphicsPipelineBuilder& SetStencilAttachmentFormat(VkFormat format);

        GraphicsPipelineBuilder& AddColorAttachmentFormat(VkFormat format);

        GraphicsPipelineBuilder& AddColorAttachmentFormat(const std::span<const VkFormat> formats);

        GraphicsPipelineBuilder& SetColorBlendConstants(float r, float g, float b, float a);

        GraphicsPipelineBuilder& SetColorBlendLogicOp(VkBool32 logicOpEnable, VkLogicOp logicOp);

        GraphicsPipelineBuilder& AddColorBlendAttachment(
            VkBool32 blendEnable, 
            VkBlendFactor srcColorBlendFactor, 
            VkBlendFactor dstColorBlendFactor,
            VkBlendOp colorBlendOp,
            VkBlendFactor srcAlphaBlendFactor,
            VkBlendFactor dstAlphaBlendFactor,
            VkBlendOp alphaBlendOp,
            VkColorComponentFlags colorWriteMask
        );

        GraphicsPipelineBuilder& AddColorBlendAttachment(const VkPipelineColorBlendAttachmentState& blendState);
        GraphicsPipelineBuilder& AddColorBlendAttachment(const std::span<const VkPipelineColorBlendAttachmentState> blendStates);

        VkPipeline Build(VkDevice vkDevice);

    private:
        enum ShaderStageIndex
        { 
            SHADER_STAGE_VERTEX,
            SHADER_STAGE_PIXEL,
            SHADER_STAGE_COUNT
        };

    private:
        static constexpr VkShaderStageFlagBits ShaderStageIndexToFlagBits(ShaderStageIndex index);

    private:
        GraphicsPipelineBuilder& SetShaderInfo(ShaderStageIndex index, VkShaderModule shader, const char* pEntryName);

    private:
        static inline constexpr size_t MAX_SHADER_ENTRY_NAME_LENGTH = 64;
        static inline constexpr size_t MAX_VERTEX_ATTRIBUTES_COUNT = 16;
        static inline constexpr size_t MAX_DYNAMIC_STATES_COUNT = 16;
        static inline constexpr size_t MAX_COLOR_ATTACHMENTS_COUNT = 8;
        static inline constexpr size_t MAX_VIEWPORT_AND_SCISSOR_COUNT = 1;

    private:
        VkPipelineVertexInputStateCreateInfo m_vertexInputState = {};
        VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyState = {};
        VkPipelineRasterizationStateCreateInfo m_rasterizationState = {};
        VkPipelineDepthStencilStateCreateInfo m_depthStencilState = {};
        VkPipelineMultisampleStateCreateInfo m_multisampleState = {};
        VkPipelineColorBlendStateCreateInfo m_colorBlendState = {};
        VkPipelineRenderingCreateInfo m_renderingCreateInfo = {};
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        VkPipelineCreateFlags m_flags = {};

        std::array<VkPipelineShaderStageCreateInfo, SHADER_STAGE_COUNT> m_shaderStages = {};
        std::array<std::array<char, MAX_SHADER_ENTRY_NAME_LENGTH + 1>, SHADER_STAGE_COUNT> m_shaderEntryNames = {};

        std::array<VkDynamicState, MAX_DYNAMIC_STATES_COUNT> m_dynamicStateValues = {};
        size_t m_dynamicStatesCount = 0;

        std::array<VkViewport, MAX_VIEWPORT_AND_SCISSOR_COUNT> m_viewports = {};
        std::array<VkRect2D, MAX_VIEWPORT_AND_SCISSOR_COUNT> m_scissors = {};
        size_t m_viewportsAndScissorCount = 0;

        std::array<VkFormat, MAX_COLOR_ATTACHMENTS_COUNT> m_colorAttachmentFormats = {};
        size_t m_colorAttachmentFormatsCount = 0;

        std::array<VkPipelineColorBlendAttachmentState, MAX_COLOR_ATTACHMENTS_COUNT> m_colorBlendAttachmentStates = {};
        size_t m_colorBlendAttachmentStatesCount = 0;
    };


    class ComputePipelineBuilder
    {
    public:
        ComputePipelineBuilder() { Reset(); }

        ComputePipelineBuilder& Reset();

        ComputePipelineBuilder& SetFlags(VkPipelineCreateFlags flags);

        ComputePipelineBuilder& SetLayout(VkPipelineLayout layout);

        ComputePipelineBuilder& SetShader(VkShaderModule shader, const char* pEntryName = "main");

        VkPipeline Build(VkDevice vkDevice);

    private:
        static inline constexpr size_t MAX_SHADER_ENTRY_NAME_LENGTH = 127;

    private:
        VkComputePipelineCreateInfo m_createInfo = {};
        std::array<char, MAX_SHADER_ENTRY_NAME_LENGTH + 1> m_shaderEntryName = {};
    };


    class PipelineLayoutBuilder
    {
    public:
        PipelineLayoutBuilder(size_t maxPushConstBlockSize);

        PipelineLayoutBuilder& Reset();

        PipelineLayoutBuilder& SetMaxPushConstBlockSize(size_t size);

        PipelineLayoutBuilder& SetFlags(VkPipelineLayoutCreateFlags flags);

        PipelineLayoutBuilder& AddPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size);

        PipelineLayoutBuilder& AddDescriptorSetLayout(VkDescriptorSetLayout vkSetLayout);

        VkPipelineLayout Build(VkDevice vkDevice);

    private:
        static inline constexpr size_t MAX_PUSH_CONSTANT_RANGE_COUNT = 32;
        static inline constexpr size_t MAX_DESCRIPTOR_SET_LAYOUT_COUNT = 8;

    private:
        std::array<VkPushConstantRange, MAX_PUSH_CONSTANT_RANGE_COUNT> m_pushConstRanges = {};
        size_t m_pushConstRangeCount = 0;

        std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SET_LAYOUT_COUNT> m_layouts = {};
        size_t m_layoutCount = 0;

        VkPipelineLayoutCreateFlags m_flags = {};
        size_t m_maxPushConstBlockSize = 0;
    };


    class DescriptorSetLayoutBuilder
    {
    public:
        DescriptorSetLayoutBuilder(size_t bindingsCount = 0);

        DescriptorSetLayoutBuilder& Reset();

        DescriptorSetLayoutBuilder& AddBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stages);

        VkDescriptorSetLayout Build(VkDevice vkDevice);

    private:
        bool IsBindingExist(uint32_t bindingNumber);

    private:
        std::vector<VkDescriptorSetLayoutBinding> m_bindings;
        VkDescriptorSetLayoutCreateFlags m_flags = 0;
    };


    class DescriptorPoolBuilder
    {
    public:
        DescriptorPoolBuilder(size_t resourcesTypesCount = 0);

        DescriptorPoolBuilder& Reset();

        DescriptorPoolBuilder& SetFlags(VkDescriptorPoolCreateFlags flags);

        DescriptorPoolBuilder& SetMaxDescriptorSetsCount(size_t count);

        DescriptorPoolBuilder& AddResource(VkDescriptorType type, uint32_t descriptorCount);

        VkDescriptorPool Build(VkDevice vkDevice);

    private:
        std::vector<VkDescriptorPoolSize> m_poolSizes;
        uint32_t m_maxDescriptorSets = 0;
        VkDescriptorPoolCreateFlags m_flags = 0;
    };


    class DescriptorSetAllocator
    {
    public:
        DescriptorSetAllocator(uint32_t layoutsCount = 0);

        DescriptorSetAllocator& Reset();

        DescriptorSetAllocator& SetPool(VkDescriptorPool vkPool);

        DescriptorSetAllocator& AddLayout(VkDescriptorSetLayout vkLayout);

        void Allocate(VkDevice vkDevice, std::span<VkDescriptorSet> outDescriptorSets);

    private:
        std::vector<VkDescriptorSetLayout> m_layouts;
        VkDescriptorPool m_vkDescPool = VK_NULL_HANDLE;
    };
}