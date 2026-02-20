#pragma once

#include "vk_object.h"


namespace vkn
{
    class Device;
    class DescriptorSetLayout;
    class Shader;

    
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


    class PSO : public Object
    {
        friend class GraphicsPSOBuilder;
        friend class ComputePSOBuilder;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(PSO);

        PSO() = default;
        ~PSO();

        PSO(PSO&& pso) noexcept;
        PSO& operator=(PSO&& pso) noexcept;

        PSO& Destroy();

        template <typename... Args>
        PSO& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_pso, VK_OBJECT_TYPE_PIPELINE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const
        {
            return Object::GetDebugName("PSO");
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pLayout->GetDevice();
        }

        const VkPipeline& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_pso;
        }

        VkPipelineBindPoint GetBindPoint() const;

        bool IsRasterization() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_RASTERIZATION_PSO);
        }

        bool IsCompute() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_COMPUTE_PSO);
        }

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

        VkPipeline m_pso = VK_NULL_HANDLE;

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

        GraphicsPSOBuilder& EnableRasterizerDepthClamp();

        GraphicsPSOBuilder& EnableRasterizerDiscard();

        GraphicsPSOBuilder& SetRasterizerPolygonMode(VkPolygonMode polygonMode);

        GraphicsPSOBuilder& SetRasterizerCullMode(VkCullModeFlags cullMode);

        GraphicsPSOBuilder& SetRasterizerFrontFace(VkFrontFace frontFace);

        GraphicsPSOBuilder& EnableRasterizerDepthBias(float biasConstantFactor, float biasClamp, float biasSlopeFactor);

        GraphicsPSOBuilder& EnableDepthTest(VkBool32 depthWriteEnable, VkCompareOp compareOp);

        GraphicsPSOBuilder& EnableStencilTestState(const VkStencilOpState& front, const VkStencilOpState& back);

        GraphicsPSOBuilder& EnableDepthBoundsTest(float minValue, float maxValue);

        GraphicsPSOBuilder& SetRenderingViewMask(uint32_t viewMask);

        GraphicsPSOBuilder& SetColorBlendConstants(float r, float g, float b, float a);

        GraphicsPSOBuilder& EnableColorBlendLogicOp(VkLogicOp logicOp);

        GraphicsPSOBuilder& SetDepthAttachmentFormat(VkFormat format);

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