#pragma once

#include "vk_object.h"


namespace vkn
{
    class Device;
    class DescriptorSetLayout;

    
    struct PSOLayoutCreateInfo
    {
        Device*                               pDevice;
        std::span<const DescriptorSetLayout*> setLayouts;
        std::span<const VkPushConstantRange>  pushConstantRanges;
        VkPipelineLayoutCreateFlags           flags;
    };


    class PSOLayout : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(PSOLayout);

        PSOLayout() = default;
        PSOLayout(const PSOLayoutCreateInfo& info);
        PSOLayout(Device* pDevice, std::span<const DescriptorSetLayout*> setLayouts, std::span<const VkPushConstantRange> pushConstantRanges = {}, VkPipelineLayoutCreateFlags flags = 0);
        
        ~PSOLayout();

        PSOLayout(PSOLayout&& layout) noexcept;
        PSOLayout& operator=(PSOLayout&& layout) noexcept;

        PSOLayout& Create(const PSOLayoutCreateInfo& info);
        PSOLayout& Create(Device* pDevice, std::span<const DescriptorSetLayout*> setLayouts, std::span<const VkPushConstantRange> pushConstantRanges = {}, VkPipelineLayoutCreateFlags flags = 0);
        PSOLayout& Destroy();

        const char* GetDebugName() const
        {
            return Object::GetDebugName("PSOLayout");
        }

        template <typename... Args>
        PSOLayout& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        const VkPipelineLayout& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_layout;
        }

    private:
        Device* m_pDevice = nullptr;

        VkPipelineLayout m_layout = VK_NULL_HANDLE;
    };





    class GraphicsPipelineBuilder
    {
    public:
        GraphicsPipelineBuilder() { Reset(); }

        GraphicsPipelineBuilder& Reset();

        GraphicsPipelineBuilder& SetFlags(VkPipelineCreateFlags flags);

        GraphicsPipelineBuilder& SetLayout(VkPipelineLayout layout);

        GraphicsPipelineBuilder& AddShader(VkShaderModule shader, VkShaderStageFlagBits stage, const char* pEntryName = "main");

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

        VkPipeline Build();

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

        std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
        std::vector<std::array<char, MAX_SHADER_ENTRY_NAME_LENGTH + 1>> m_shaderEntryNames = {};

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

        VkPipeline Build();

    private:
        static inline constexpr size_t MAX_SHADER_ENTRY_NAME_LENGTH = 127;

    private:
        VkComputePipelineCreateInfo m_createInfo = {};
        std::array<char, MAX_SHADER_ENTRY_NAME_LENGTH + 1> m_shaderEntryName = {};
    };

}