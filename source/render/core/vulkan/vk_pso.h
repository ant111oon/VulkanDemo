#pragma once

#include "vk_descriptor.h"
#include "vk_shader.h"


namespace vkn
{    
    struct PSOLayoutCreateInfo
    {
        Device*                               pDevice;
        std::span<const DescriptorSetLayout*> setLayouts;
        std::span<const VkPushConstantRange>  pushConstantRanges;
        VkPipelineLayoutCreateFlags           flags;
    };


    class PSOLayout : public Handle<VkPipelineLayout>
    {
    public:
        using Base = Handle<VkPipelineLayout>;

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

        Device& GetDevice() const;

    private:
        Device* m_pDevice = nullptr;
    };


    class PSO : public Handle<VkPipeline>
    {
        friend class GraphicsPSOBuilder;
        friend class ComputePSOBuilder;

    public:
        using Base = Handle<VkPipeline>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(PSO);

        PSO() = default;
        ~PSO();

        PSO(PSO&& pso) noexcept;
        PSO& operator=(PSO&& pso) noexcept;

        PSO& Destroy();

        Device& GetDevice() const;

        PSOLayout& GetLayout() const;

        VkPipelineBindPoint GetBindPoint() const;

        bool IsRasterization() const;
        bool IsCompute() const;

    private:
        enum StateBits
        {
            BIT_IS_RASTERIZATION_PSO,
            BIT_IS_COMPUTE_PSO,
            BIT_COUNT,
        };

        using State = std::bitset<BIT_COUNT>;

        PSO(PSOLayout* pLayout, VkPipeline pso, State state);

        PSO& Create(PSOLayout* pLayout, VkPipeline pso, State state);

    private:
        PSOLayout* m_pLayout = nullptr;

        State m_state = {};
    };


    class GraphicsPSOBuilder
    {
    public:
        GraphicsPSOBuilder() { Reset(); }

        GraphicsPSOBuilder& Reset();

        GraphicsPSOBuilder& SetFlags(VkPipelineCreateFlags flags);

        GraphicsPSOBuilder& SetLayout(PSOLayout& layout);

        GraphicsPSOBuilder& AddShader(vkn::Shader& shader);

        GraphicsPSOBuilder& SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);

        GraphicsPSOBuilder& AddDynamicState(VkDynamicState state);

        GraphicsPSOBuilder& AddDynamicState(const std::span<const VkDynamicState> states);

        GraphicsPSOBuilder& AddViewportAndScissor(const VkViewport& viewport, const VkRect2D& scissor);

        GraphicsPSOBuilder& SetRasterizerLineWidth(float lineWidth);

        GraphicsPSOBuilder& SetRasterizerDepthClampState(VkBool32 enabled);

        GraphicsPSOBuilder& SetRasterizerDiscardState(VkBool32 rasterizerDiscardEnable);

        GraphicsPSOBuilder& SetRasterizerPolygonMode(VkPolygonMode polygonMode);

        GraphicsPSOBuilder& SetRasterizerCullMode(VkCullModeFlags cullMode);

        GraphicsPSOBuilder& SetRasterizerFrontFace(VkFrontFace frontFace);

        GraphicsPSOBuilder& SetRasterizerDepthBiasState(VkBool32 enabled, float biasConstantFactor, float biasClamp, float biasSlopeFactor);

        GraphicsPSOBuilder& SetDepthTestState(VkBool32 enabled, VkCompareOp compareOp);

        GraphicsPSOBuilder& SetDepthWriteState(VkBool32 enabled);

        GraphicsPSOBuilder& SetStencilTestState(VkBool32 enabled, const VkStencilOpState& front, const VkStencilOpState& back);

        GraphicsPSOBuilder& SetDepthBoundsTestState(VkBool32 enabled, float minValue, float maxValue);

        GraphicsPSOBuilder& SetRenderingViewMask(uint32_t viewMask);

        GraphicsPSOBuilder& SetColorBlendConstants(float r, float g, float b, float a);

        GraphicsPSOBuilder& SetColorBlendLogicOpState(VkBool32 enabled, VkLogicOp logicOp);

        GraphicsPSOBuilder& SetDepthAttachment(VkFormat format);

        GraphicsPSOBuilder& SetStencilAttachmentFormat(VkFormat format);

        GraphicsPSOBuilder& AddColorAttachment(
            VkFormat format,
            VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, 
            VkBool32 blendEnable = VK_FALSE,
            VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, 
            VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            VkBlendOp colorBlendOp = VK_BLEND_OP_ADD,
            VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD            
        );

        PSO Build();

    private:
        VkPipelineVertexInputStateCreateInfo    m_vertexInputState = {};
        VkPipelineInputAssemblyStateCreateInfo  m_inputAssemblyState = {};
        VkPipelineRasterizationStateCreateInfo  m_rasterizationState = {};
        VkPipelineDepthStencilStateCreateInfo   m_depthStencilState = {};
        VkPipelineMultisampleStateCreateInfo    m_multisampleState = {};
        VkPipelineColorBlendStateCreateInfo     m_colorBlendState = {};
        VkPipelineRenderingCreateInfo           m_renderingCreateInfo = {};
        PSOLayout*                              m_pLayout = nullptr;
        VkPipelineCreateFlags                   m_flags = {};
        std::vector<VkViewport>                 m_viewports = {};
        std::vector<VkRect2D>                   m_scissors = {};
        std::vector<VkFormat>                   m_colorAttachmentFormats = {};
        std::vector<VkDynamicState>             m_dynamicStateValues = {};
        std::vector<VkPipelineShaderStageCreateInfo>     m_shaderStages = {};
        std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendAttachmentStates = {};
    };


    class ComputePSOBuilder
    {
    public:
        ComputePSOBuilder() { Reset(); }

        ComputePSOBuilder& Reset();

        ComputePSOBuilder& SetFlags(VkPipelineCreateFlags flags);
        ComputePSOBuilder& SetLayout(PSOLayout& layout);
        ComputePSOBuilder& SetShader(Shader& shader);

        PSO Build();

    private:
        VkComputePipelineCreateInfo m_createInfo = {};
        PSOLayout* m_pLayout = nullptr;
    };
}