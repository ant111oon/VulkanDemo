#include "core/wnd_system/wnd_system.h"

#include "core/platform/file/file.h"
#include "core/utils/timer.h"

#include "render/core/vulkan/vk_instance.h"
#include "render/core/vulkan/vk_surface.h"
#include "render/core/vulkan/vk_phys_device.h"
#include "render/core/vulkan/vk_device.h"
#include "render/core/vulkan/vk_swapchain.h"
#include "render/core/vulkan/vk_buffer.h"
#include "render/core/vulkan/vk_fence.h"

#include <glm/glm.hpp>


namespace fs = std::filesystem;


struct TestVertex
{
    glm::vec2 ndc;
    glm::vec2 uv;
    glm::vec4 color;
};

static constexpr std::array TEST_VERTECIES = {
    TestVertex { glm::vec2(-0.5f, 0.5f), glm::vec2(0.f,  0.f), glm::vec4(1.f, 0.f, 0.f, 1.f) },
    TestVertex { glm::vec2( 0.5f, 0.5f), glm::vec2(1.f,  0.f), glm::vec4(0.f, 1.f, 0.f, 1.f) },
    TestVertex { glm::vec2( 0.f, -0.5f), glm::vec2(0.5f, 1.f), glm::vec4(0.f, 0.f, 1.f, 1.f) },
};


static constexpr size_t VERTEX_BUFFER_SIZE_F4 = 4096;
static constexpr size_t VERTEX_BUFFER_SIZE_BYTES = VERTEX_BUFFER_SIZE_F4 * sizeof(glm::vec4);

static constexpr const char* APP_NAME = "Vulkan Demo";

static constexpr bool VSYNC_ENABLED = false;


static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();

static vkn::Device& s_vkDevice = vkn::GetDevice();

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();

static VkCommandPool   s_vkCmdPool = VK_NULL_HANDLE;
static VkCommandBuffer s_vkImmediateSubmitCmdBuffer = VK_NULL_HANDLE;

static VkDescriptorPool      s_vkDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet       s_vkDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_vkDescriptorSetLayout = VK_NULL_HANDLE;

static VkPipelineLayout      s_vkPipelineLayout = VK_NULL_HANDLE;
static VkPipeline            s_vkPipeline = VK_NULL_HANDLE;

static std::vector<VkSemaphore>     s_vkPresentFinishedSemaphores;
static std::vector<VkSemaphore>     s_vkRenderingFinishedSemaphores;
static std::vector<vkn::Fence>      s_vkRenderingFinishedFences;
static std::vector<VkCommandBuffer> s_vkRenderCmdBuffers;

static vkn::Fence s_vkImmediateSubmitFinishedFence;

static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_commonConstBuffer;

static size_t s_frameNumber = 0;
static bool s_swapchainRecreateRequired = false;


class GraphicsPipelineBuilder
{
private:
    enum ShaderStageIndex
    { 
        SHADER_STAGE_VERTEX,
        SHADER_STAGE_PIXEL,
        SHADER_STAGE_COUNT
    };

public:
    GraphicsPipelineBuilder()
    {
        Reset();
    }

    GraphicsPipelineBuilder& Reset()
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

    GraphicsPipelineBuilder& SetFlags(VkPipelineCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }

    GraphicsPipelineBuilder& SetLayout(VkPipelineLayout layout)
    {
        m_layout = layout;
        return *this;
    }

    GraphicsPipelineBuilder& SetVertexShader(VkShaderModule shader, const char* pEntryName = "main")
    {
        return SetShaderInfo(SHADER_STAGE_VERTEX, shader, pEntryName);
    }

    GraphicsPipelineBuilder& SetPixelShader(VkShaderModule shader, const char* pEntryName = "main")
    {
        return SetShaderInfo(SHADER_STAGE_PIXEL, shader, pEntryName);
    }

    GraphicsPipelineBuilder& SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE)
    {
        m_inputAssemblyState.topology = topology;
        m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;

        return *this;
    }

    GraphicsPipelineBuilder& AddDynamicState(VkDynamicState state)
    {
        CORE_ASSERT(m_dynamicStatesCount + 1 <= MAX_DYNAMIC_STATES_COUNT);
        m_dynamicStateValues[m_dynamicStatesCount++] = state;

        return *this;
    }

    GraphicsPipelineBuilder& AddDynamicState(const std::span<const VkDynamicState> states)
    {
        for (VkDynamicState state : states) {
            AddDynamicState(state);
        }

        return *this;
    }

    GraphicsPipelineBuilder& AddViewportAndScissor(const VkViewport& viewport, const VkRect2D& scissor)
    {
        CORE_ASSERT(m_viewportsAndScissorCount + 1 <= MAX_VIEWPORT_AND_SCISSOR_COUNT);

        m_viewports[m_viewportsAndScissorCount] = viewport;
        m_scissors[m_viewportsAndScissorCount] = scissor;
        
        ++m_viewportsAndScissorCount;

        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerLineWidth(float lineWidth)
    {
        m_rasterizationState.lineWidth = lineWidth;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerDepthClampEnabled(VkBool32 enabled)
    {
        m_rasterizationState.depthClampEnable = enabled;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerDiscardEnabled(VkBool32 enabled)
    {
        m_rasterizationState.rasterizerDiscardEnable = enabled;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerPolygonMode(VkPolygonMode polygonMode)
    {
        m_rasterizationState.polygonMode = polygonMode;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerCullMode(VkCullModeFlags cullMode)
    {
        m_rasterizationState.cullMode = cullMode;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerFrontFace(VkFrontFace frontFace)
    {
        m_rasterizationState.frontFace = frontFace;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerDepthBias(VkBool32 enabled, float biasConstantFactor, float biasClamp, float biasSlopeFactor)
    {
        m_rasterizationState.depthBiasEnable = enabled;
        m_rasterizationState.depthBiasConstantFactor = biasConstantFactor;
        m_rasterizationState.depthBiasClamp = biasClamp;
        m_rasterizationState.depthBiasSlopeFactor = biasSlopeFactor;

        return *this;
    }

    GraphicsPipelineBuilder& SetDepthTestState(VkBool32 testEnabled, VkBool32 depthWriteEnable, VkCompareOp compareOp)
    {
        m_depthStencilState.depthTestEnable = testEnabled;
        m_depthStencilState.depthWriteEnable = depthWriteEnable;
        m_depthStencilState.depthCompareOp = compareOp;
        return *this;
    }

    GraphicsPipelineBuilder& SetStencilTestState(VkBool32 testEnabled, const VkStencilOpState& front, const VkStencilOpState& back)
    {
        m_depthStencilState.stencilTestEnable = testEnabled;
        m_depthStencilState.front = front;
        m_depthStencilState.back = back;
        return *this;
    }

    GraphicsPipelineBuilder& SetDepthBoundsTestState(VkBool32 depthBoundsTestEnable, float minValue, float maxValue)
    {
        m_depthStencilState.depthBoundsTestEnable = depthBoundsTestEnable;
        m_depthStencilState.minDepthBounds = minValue;
        m_depthStencilState.maxDepthBounds = maxValue;
        return *this;
    }

    GraphicsPipelineBuilder& SetRenderingViewMask(uint32_t viewMask)
    {
        m_renderingCreateInfo.viewMask = viewMask;
        return *this;
    }

    GraphicsPipelineBuilder& SetRenderingDepthAttachmentFormat(VkFormat format)
    {
        m_renderingCreateInfo.depthAttachmentFormat = format;
        return *this;
    }

    GraphicsPipelineBuilder& SetRenderingStencilAttachmentFormat(VkFormat format)
    {
        m_renderingCreateInfo.stencilAttachmentFormat = format;
        return *this;
    }


    GraphicsPipelineBuilder& AddColorAttachmentFormat(VkFormat format)
    {
        CORE_ASSERT(m_colorAttachmentFormatsCount + 1 <= MAX_COLOR_ATTACHMENTS_COUNT);
        m_colorAttachmentFormats[m_colorAttachmentFormatsCount++] = format;

        return *this;
    }

    GraphicsPipelineBuilder& AddColorAttachmentFormat(const std::span<const VkFormat> formats)
    {
        for (VkFormat format : formats) {
            AddColorAttachmentFormat(format);
        }
        
        return *this;
    }

    GraphicsPipelineBuilder& SetColorBlendConstants(float r, float g, float b, float a)
    {
        m_colorBlendState.blendConstants[0] = r;
        m_colorBlendState.blendConstants[1] = g;
        m_colorBlendState.blendConstants[2] = b;
        m_colorBlendState.blendConstants[3] = a;
        return *this;
    }

    GraphicsPipelineBuilder& SetColorBlendLogicOp(VkBool32 logicOpEnable, VkLogicOp logicOp)
    {
        m_colorBlendState.logicOpEnable = logicOpEnable;
        m_colorBlendState.logicOp = logicOp;
        return *this;
    }

    GraphicsPipelineBuilder& AddColorBlendAttachment(
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

    GraphicsPipelineBuilder& AddColorBlendAttachment(const VkPipelineColorBlendAttachmentState& blendState)
    {
        CORE_ASSERT(m_colorBlendAttachmentStatesCount + 1 <= MAX_COLOR_ATTACHMENTS_COUNT);
        m_colorBlendAttachmentStates[m_colorBlendAttachmentStatesCount++] = blendState;

        return *this;
    }

    GraphicsPipelineBuilder& AddColorBlendAttachment(const std::span<const VkPipelineColorBlendAttachmentState> blendStates)
    {
        for (const VkPipelineColorBlendAttachmentState& blendState : blendStates) {
            AddColorBlendAttachment(blendState);
        }

        return *this;
    }

    VkPipeline Build(VkDevice vkDevice)
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

private:
    static constexpr VkShaderStageFlagBits ShaderStageIndexToFlagBits(ShaderStageIndex index)
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

private:
    GraphicsPipelineBuilder& SetShaderInfo(ShaderStageIndex index, VkShaderModule shader, const char* pEntryName)
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
    ComputePipelineBuilder()
    {
        Reset();
    }

    ComputePipelineBuilder& Reset()
    {
        m_createInfo = {};
        m_createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        m_createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        m_shaderEntryName.fill('\0');

        return *this;
    }

    ComputePipelineBuilder& SetFlags(VkPipelineCreateFlags flags)
    {
        m_createInfo.flags = flags;
        return *this;
    }

    ComputePipelineBuilder& SetLayout(VkPipelineLayout layout)
    {
        m_createInfo.layout = layout;
        return *this;
    }

    ComputePipelineBuilder& SetShader(VkShaderModule shader, const char* pEntryName = "main")
    {
        CORE_ASSERT(pEntryName && strlen(pEntryName) <= MAX_SHADER_ENTRY_NAME_LENGTH);
        strcpy_s(m_shaderEntryName.data(), MAX_SHADER_ENTRY_NAME_LENGTH, pEntryName);

        m_createInfo.stage.module = shader;
        m_createInfo.stage.pName = m_shaderEntryName.data();
        m_createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

        return *this;
    }

    VkPipeline Build(VkDevice vkDevice)
    {
        CORE_ASSERT(m_createInfo.layout);
        CORE_ASSERT(m_createInfo.stage.module);

        VkPipeline vkPipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(vkDevice, VK_NULL_HANDLE, 1, &m_createInfo, nullptr, &vkPipeline));
        VK_ASSERT(vkPipeline != VK_NULL_HANDLE);

        return vkPipeline;
    }

private:
    static inline constexpr size_t MAX_SHADER_ENTRY_NAME_LENGTH = 64;

private:
    VkComputePipelineCreateInfo m_createInfo = {};
    std::array<char, MAX_SHADER_ENTRY_NAME_LENGTH + 1> m_shaderEntryName = {};
};


class PipelineLayoutBuilder
{
public:
    PipelineLayoutBuilder(size_t maxPushConstBlockSize)
    {
        Reset();
        SetMaxPushConstBlockSize(maxPushConstBlockSize);
    }

    PipelineLayoutBuilder& Reset()
    {
        m_pushConstRanges.fill({});
        m_pushConstRangeCount = 0;

        m_layouts.fill(VK_NULL_HANDLE);
        m_layoutCount = 0;

        m_flags = {};
        m_maxPushConstBlockSize = 0;

        return *this;
    }

    PipelineLayoutBuilder& SetMaxPushConstBlockSize(size_t size)
    {
        m_maxPushConstBlockSize = size;
        return *this;
    }

    PipelineLayoutBuilder& SetFlags(VkPipelineLayoutCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }

    PipelineLayoutBuilder& AddPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size)
    {
        VK_ASSERT(m_pushConstRangeCount + 1 <= MAX_PUSH_CONSTANT_RANGE_COUNT);
        VK_ASSERT(offset + size <= m_maxPushConstBlockSize);
        
        m_pushConstRanges[m_pushConstRangeCount].stageFlags = stages;
        m_pushConstRanges[m_pushConstRangeCount].offset = offset;
        m_pushConstRanges[m_pushConstRangeCount].size = size;

        ++m_pushConstRangeCount;

        return *this;
    }

    PipelineLayoutBuilder& AddDescriptorSetLayout(VkDescriptorSetLayout vkSetLayout)
    {
        VK_ASSERT(vkSetLayout != VK_NULL_HANDLE);
        VK_ASSERT(m_layoutCount + 1 <= MAX_DESCRIPTOR_SET_LAYOUT_COUNT);
        
        m_layouts[m_layoutCount] = vkSetLayout;

        ++m_layoutCount;

        return *this;
    }

    VkPipelineLayout Build(VkDevice vkDevice)
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
    DescriptorSetLayoutBuilder(size_t bindingsCount = 0)
    {
        Reset();
        m_bindings.reserve(bindingsCount);
    }

    DescriptorSetLayoutBuilder& Reset()
    {
        m_bindings.clear();
        m_flags = 0;

        return *this;
    }

    DescriptorSetLayoutBuilder& AddBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stages)
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

    VkDescriptorSetLayout Build(VkDevice vkDevice)
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

private:
    bool IsBindingExist(uint32_t bindingNumber)
    {
        return std::find_if(m_bindings.cbegin(), m_bindings.cend(), [bindingNumber](const VkDescriptorSetLayoutBinding& binding){
            return binding.binding == bindingNumber;
        }) != m_bindings.cend();
    }

private:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
    VkDescriptorSetLayoutCreateFlags m_flags = 0;
};


class DescriptorPoolBuilder
{
public:
    DescriptorPoolBuilder(size_t resourcesTypesCount = 0)
    {
        Reset();
        m_poolSizes.reserve(resourcesTypesCount);
    }

    DescriptorPoolBuilder& Reset()
    {
        m_poolSizes.clear();
        m_maxDescriptorSets = 0;
        m_flags = 0;

        return *this;
    }

    DescriptorPoolBuilder& SetFlags(VkDescriptorPoolCreateFlags flags)
    {
        m_flags = flags;
        return *this;
    }

    DescriptorPoolBuilder& SetMaxDescriptorSetsCount(size_t count)
    {
        m_maxDescriptorSets = count;
        return *this;
    }

    DescriptorPoolBuilder& AddResource(VkDescriptorType type, uint32_t descriptorCount)
    {
        VkDescriptorPoolSize descPoolSize = {};
        descPoolSize.type = type;
        descPoolSize.descriptorCount = descriptorCount;

        m_poolSizes.emplace_back(descPoolSize);

        return *this;
    }

    VkDescriptorPool Build(VkDevice vkDevice)
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

private:
    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_maxDescriptorSets = 0;
    VkDescriptorPoolCreateFlags m_flags = 0;
};


class DescriptorSetAllocator
{
public:
    DescriptorSetAllocator(uint32_t layoutsCount = 0)
    {
        Reset();
        m_layouts.reserve(layoutsCount);
    }

    DescriptorSetAllocator& Reset()
    {
        m_layouts.clear();
        m_vkDescPool = VK_NULL_HANDLE;

        return *this;
    }

    DescriptorSetAllocator& SetPool(VkDescriptorPool vkPool)
    {
        VK_ASSERT(vkPool != VK_NULL_HANDLE);
        m_vkDescPool = vkPool;
        
        return *this;
    }

    DescriptorSetAllocator& AddLayout(VkDescriptorSetLayout vkLayout)
    {
        VK_ASSERT(vkLayout != VK_NULL_HANDLE);
        m_layouts.emplace_back(vkLayout);
        
        return *this;
    }

    void Allocate(VkDevice vkDevice, std::span<VkDescriptorSet> outDescriptorSets)
    {
        VK_ASSERT(outDescriptorSets.size() >= m_layouts.size());

        VkDescriptorSetAllocateInfo descSetAllocInfo = {};
        descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetAllocInfo.descriptorPool = m_vkDescPool;
        descSetAllocInfo.descriptorSetCount = m_layouts.size();
        descSetAllocInfo.pSetLayouts = m_layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(vkDevice, &descSetAllocInfo, outDescriptorSets.data()));
    }

private:
    std::vector<VkDescriptorSetLayout> m_layouts;
    VkDescriptorPool m_vkDescPool = VK_NULL_HANDLE;
};


#ifdef ENG_BUILD_DEBUG
static VkBool32 VKAPI_PTR DbgVkMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        return VK_FALSE;
    }

    const char* pType = "UNKNOWN TYPE";

    switch(messageTypes) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            pType = "GENERAL";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            pType = "VALIDATION";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            pType = "PERFORMANCE";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
            pType = "DEVICE ADDR BINDING";
            break;
        default:
            VK_ASSERT_FAIL("Invalid message type");
            break;
    }

    switch(messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            ENG_LOG_TRACE("VULKAN", "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            ENG_LOG_INFO("VULKAN", "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            ENG_LOG_WARN("VULKAN", "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            VK_ASSERT_MSG(false, "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        default:
            VK_ASSERT_FAIL("Invalid message severity");
            break;
    }

    return VK_FALSE;
}
#endif


VkCommandPool CreateVkCmdPool(VkDevice vkDevice, uint32_t queueFamilyIndex)
{
    Timer timer;

    VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(vkDevice, &cmdPoolCreateInfo, nullptr, &cmdPool));
    VK_ASSERT(cmdPool != VK_NULL_HANDLE);

    VK_LOG_INFO("VkCommandPool initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return cmdPool;
}


static VkCommandBuffer AllocateVkCmdBuffer(VkDevice vkDevice, VkCommandPool vkCmdPool)
{
    Timer timer;

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
    cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocInfo.commandPool = vkCmdPool;
    cmdBufferAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(vkDevice, &cmdBufferAllocInfo, &cmdBuffer));
    VK_ASSERT(cmdBuffer != VK_NULL_HANDLE);

    VK_LOG_INFO("VkCommandBuffer allocating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return cmdBuffer;
}


static VkShaderModule CreateVkShaderModule(VkDevice vkDevice, const fs::path& shaderSpirVPath, std::vector<uint8_t>* pExternalBuffer = nullptr)
{
    Timer timer;

    std::vector<uint8_t>* pShaderData = nullptr;
    std::vector<uint8_t> localBuffer;
    
    pShaderData = pExternalBuffer ? pExternalBuffer : &localBuffer;

    if (!ReadFile(*pShaderData, shaderSpirVPath)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", shaderSpirVPath.string().c_str());
    }
    VK_ASSERT_MSG(pShaderData->size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", 
        shaderSpirVPath.string().c_str(), sizeof(uint32_t));

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(pShaderData->data());
    shaderModuleCreateInfo.codeSize = pShaderData->size();

    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &vkShaderModule));
    VK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

    VK_LOG_INFO("Shader module \"%s\" creating finished: %f ms", shaderSpirVPath.string().c_str(), timer.End().GetDuration<float, std::milli>());

    return vkShaderModule;
}


static VkDescriptorPool CreateVkDescriptorPool(VkDevice vkDevice)
{
    Timer timer;

    DescriptorPoolBuilder builder;

    VkDescriptorPool vkPool = builder.SetMaxDescriptorSetsCount(5)
        .AddResource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
        .Build(vkDevice);

    VK_LOG_INFO("VkDescriptorPool creating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPool;
}


static VkDescriptorSetLayout CreateVkDescriptorSetLayout(VkDevice vkDevice)
{
    Timer timer;

    DescriptorSetLayoutBuilder builder;

    VkDescriptorSetLayout vkLayout = builder
        .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
        .Build(vkDevice);

    VK_LOG_INFO("VkDescriptorSetLayout creating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkLayout;
}


static VkDescriptorSet CreateVkDescriptorSet(VkDevice vkDevice, VkDescriptorPool vkDescriptorPool, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    Timer timer;

    DescriptorSetAllocator allocator;

    VkDescriptorSet vkDescriptorSets[] = { VK_NULL_HANDLE };

    allocator.SetPool(vkDescriptorPool)
        .AddLayout(vkDescriptorSetLayout)
        .Allocate(vkDevice, vkDescriptorSets);

    VK_LOG_INFO("VkDescriptorSet allocating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkDescriptorSets[0];
}


static VkPipelineLayout CreateVkPipelineLayout(VkDevice vkDevice, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    Timer timer;

    PipelineLayoutBuilder plBuilder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    VkPipelineLayout vkLayout = plBuilder
        .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress))
        .AddDescriptorSetLayout(vkDescriptorSetLayout)
        .Build(vkDevice);

    VK_LOG_INFO("VkPipelineLayout initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkLayout;
}


static VkPipeline CreateVkGraphicsPipeline(VkDevice vkDevice, VkPipelineLayout vkLayout, const fs::path& vsPath, const fs::path& psPath)
{
    Timer timer;

    static constexpr size_t SHADER_STAGES_COUNT = 2;

    std::vector<uint8_t> shaderCodeBuffer;
    std::array<VkShaderModule, SHADER_STAGES_COUNT> vkShaderModules = {
        CreateVkShaderModule(vkDevice, vsPath, &shaderCodeBuffer),
        CreateVkShaderModule(vkDevice, psPath, &shaderCodeBuffer),
    };

    GraphicsPipelineBuilder builder;
    
    VkPipeline vkPipeline = builder
        .SetVertexShader(vkShaderModules[0], "main")
        .SetPixelShader(vkShaderModules[1], "main")
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .SetDepthTestState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL)
        .SetStencilTestState(VK_FALSE, {}, {})
        .SetDepthBoundsTestState(VK_FALSE, 0.f, 1.f)
        .AddColorAttachmentFormat(s_vkSwapchain.GetImageFormat())
        .AddColorBlendAttachment(VkPipelineColorBlendAttachmentState{ .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT})
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetLayout(vkLayout)
        .Build(vkDevice);

    for (VkShaderModule& shader : vkShaderModules) {
        vkDestroyShaderModule(vkDevice, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    VK_LOG_INFO("VkPipeline (graphics) initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPipeline;
}


VkSemaphore CreateVkSemaphore()
{
    Timer timer;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(s_vkDevice.Get(), &semaphoreCreateInfo, nullptr, &vkSemaphore));
    VK_ASSERT(vkSemaphore != VK_NULL_HANDLE);

    VK_LOG_INFO("VkSemaphore initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkSemaphore;
}


VkFence CreateVkFence()
{
    Timer timer;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence vkFence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(s_vkDevice.Get(), &fenceCreateInfo, nullptr, &vkFence));
    VK_ASSERT(vkFence != VK_NULL_HANDLE);

    VK_LOG_INFO("VkFence initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkFence;
}


static void CmdPipelineImageBarrier(
    VkCommandBuffer cmdBuffer, 
    VkImageLayout oldLayout, 
    VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStageMask, 
    VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2 srcAccessMask, 
    VkAccessFlags2 dstAccessMask,
    VkImage image,
    VkImageAspectFlags aspectMask
) {
    VkImageMemoryBarrier2 imageBarrier2 = {};
    imageBarrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier2.srcStageMask = srcStageMask;
    imageBarrier2.srcAccessMask = srcAccessMask;
    imageBarrier2.dstStageMask = dstStageMask;
    imageBarrier2.dstAccessMask = dstAccessMask;
    imageBarrier2.oldLayout = oldLayout;
    imageBarrier2.newLayout = newLayout;
    imageBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier2.image = image;
    imageBarrier2.subresourceRange.aspectMask = aspectMask;
    imageBarrier2.subresourceRange.baseMipLevel = 0;
    imageBarrier2.subresourceRange.baseArrayLayer = 0;
    imageBarrier2.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageBarrier2.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo vkDependencyInfo = {};
    vkDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDependencyInfo.imageMemoryBarrierCount = 1;
    vkDependencyInfo.pImageMemoryBarriers = &imageBarrier2;

    vkCmdPipelineBarrier2(cmdBuffer, &vkDependencyInfo);
}


static void SubmitVkQueue(VkQueue vkQueue,
    VkCommandBuffer vkCmdBuffer,
    VkFence vkFinishFence,
    VkSemaphore vkWaitSemaphore, 
    VkPipelineStageFlags2 waitSemaphoreStageMask,
    VkSemaphore vkSignalSemaphore, 
    VkPipelineStageFlags2 signalSemaphoreStageMask
) {
    VkSemaphoreSubmitInfo waitSemaphoreInfo = {};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = vkWaitSemaphore;
    waitSemaphoreInfo.value = 0;
    waitSemaphoreInfo.stageMask = waitSemaphoreStageMask;
    waitSemaphoreInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreInfo = {};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = vkSignalSemaphore;
    signalSemaphoreInfo.value = 0;
    signalSemaphoreInfo.stageMask = signalSemaphoreStageMask;
    signalSemaphoreInfo.deviceIndex = 0;
    
    VkCommandBufferSubmitInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfo.commandBuffer = vkCmdBuffer;
    commandBufferInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo2 = {};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.waitSemaphoreInfoCount = vkWaitSemaphore != VK_NULL_HANDLE ? 1 : 0;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &commandBufferInfo;
    submitInfo2.signalSemaphoreInfoCount = vkSignalSemaphore != VK_NULL_HANDLE ? 1 : 0;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    VK_CHECK(vkQueueSubmit2(vkQueue, 1, &submitInfo2, vkFinishFence));
}


template <typename Func, typename... Args>
static void ImmediateSubmitQueue(VkQueue vkQueue, Func func, Args&&... args)
{   
    s_vkImmediateSubmitFinishedFence.Reset();
    VK_CHECK(vkResetCommandBuffer(s_vkImmediateSubmitCmdBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(s_vkImmediateSubmitCmdBuffer, &cmdBeginInfo));
        func(s_vkImmediateSubmitCmdBuffer, std::forward<Args>(args)...);
    VK_CHECK(vkEndCommandBuffer(s_vkImmediateSubmitCmdBuffer));

    SubmitVkQueue(
        vkQueue, 
        s_vkImmediateSubmitCmdBuffer, 
        s_vkImmediateSubmitFinishedFence.Get(), 
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE,
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE
    );

    s_vkImmediateSubmitFinishedFence.WaitFor(UINT64_MAX);
}


static bool ResizeVkSwapchain(BaseWindow* pWnd)
{
    if (!s_swapchainRecreateRequired) {
        return false;
    }

    const bool resizeResult = s_vkSwapchain.Resize(pWnd->GetWidth(), pWnd->GetHeight());
    
    s_swapchainRecreateRequired = !resizeResult;

    return s_swapchainRecreateRequired;
}


void ProcessWndEvents(const WndEvent& event)
{
    if (event.Is<WndResizeEvent>()) {
        s_swapchainRecreateRequired = true;
    }
}


struct COMMON_CB_DATA
{
    glm::vec3 color;
    float PAD0;
};


void RenderScene()
{
    const size_t frameInFlightIdx = s_frameNumber % s_vkSwapchain.GetImageCount();

    VkCommandBuffer& cmdBuffer              = s_vkRenderCmdBuffers[frameInFlightIdx];
    vkn::Fence& renderingFinishedFence      = s_vkRenderingFinishedFences[frameInFlightIdx];
    VkSemaphore& presentFinishedSemaphore   = s_vkPresentFinishedSemaphores[frameInFlightIdx];
    VkSemaphore& renderingFinishedSemaphore = s_vkRenderingFinishedSemaphores[frameInFlightIdx];

    const VkResult renderingFinishedFenceStatus = vkGetFenceStatus(s_vkDevice.Get(), renderingFinishedFence.Get());
    if (renderingFinishedFenceStatus == VK_NOT_READY) {
        return;
    }

    VK_CHECK(renderingFinishedFenceStatus);

    uint32_t nextImageIdx;
    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), UINT64_MAX, presentFinishedSemaphore, VK_NULL_HANDLE, &nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }

    renderingFinishedFence.Reset();
    VK_CHECK(vkResetCommandBuffer(cmdBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImage rndImage = s_vkSwapchain.GetImage(nextImageIdx);

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo));
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            rndImage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.extent = s_vkSwapchain.GetImageExtent();
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;

        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = s_vkSwapchain.GetImageView(nextImageIdx);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = 245.f / 255.f;
        colorAttachment.clearValue.color.float32[1] = 245.f / 255.f;
        colorAttachment.clearValue.color.float32[2] = 220.f / 255.f;
        colorAttachment.clearValue.color.float32[3] = 255.f / 255.f;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(cmdBuffer, &renderingInfo);
            VkViewport viewport = {};
            viewport.width = renderingInfo.renderArea.extent.width;
            viewport.height = renderingInfo.renderArea.extent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.extent = renderingInfo.renderArea.extent;
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipeline);
            
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipelineLayout, 0, 1, &s_vkDescriptorSet, 0, nullptr);

            const VkDeviceAddress vertBufferAddress = s_vertexBuffer.GetDeviceAddress();
            vkCmdPushConstants(cmdBuffer, s_vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &vertBufferAddress);

            vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        vkCmdEndRendering(cmdBuffer);

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_NONE,
            rndImage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    SubmitVkQueue(
        s_vkDevice.GetQueue(),
        cmdBuffer,
        renderingFinishedFence.Get(),
        presentFinishedSemaphore,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        renderingFinishedSemaphore,
        VK_PIPELINE_STAGE_2_NONE
    );

    VkSwapchainKHR vkSwapchain = s_vkSwapchain.Get();

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vkSwapchain;
    presentInfo.pImageIndices = &nextImageIdx;
    presentInfo.pResults = nullptr;
    const VkResult presentResult = vkQueuePresentKHR(s_vkDevice.GetQueue(), &presentInfo);

    if (presentResult != VK_SUBOPTIMAL_KHR && presentResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(presentResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }

    ++s_frameNumber;
}


int main(int argc, char* argv[])
{
    wndSysInit();
    BaseWindow* pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = APP_NAME;
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;
    wndInitInfo.isVisible = false;

    pWnd->Create(wndInitInfo);
    ENG_ASSERT(pWnd->IsInitialized());

#ifdef ENG_BUILD_DEBUG
    vkn::InstanceDebugMessengerCreateInfo vkDbgMessengerCreateInfo = {};
    vkDbgMessengerCreateInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vkDbgMessengerCreateInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vkDbgMessengerCreateInfo.pMessageCallback = DbgVkMessageCallback;

    constexpr std::array vkInstLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    constexpr std::array vkInstExtensions = {
    #ifdef ENG_BUILD_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef ENG_OS_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #endif
    };

    vkn::InstanceCreateInfo vkInstCreateInfo = {};
    vkInstCreateInfo.pApplicationName = APP_NAME;
    vkInstCreateInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vkInstCreateInfo.pEngineName = "VkEngine";
    vkInstCreateInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkInstCreateInfo.apiVersion = VK_API_VERSION_1_3;
    vkInstCreateInfo.extensions = vkInstExtensions;
#ifdef ENG_BUILD_DEBUG
    vkInstCreateInfo.layers = vkInstLayers;
    vkInstCreateInfo.pDbgMessengerCreateInfo = &vkDbgMessengerCreateInfo;
#endif

    s_vkInstance.Create(vkInstCreateInfo); 
    CORE_ASSERT(s_vkInstance.IsCreated()); 
    

    vkn::SurfaceCreateInfo vkSurfCreateInfo = {};
    vkSurfCreateInfo.pInstance = &s_vkInstance;
    vkSurfCreateInfo.pWndHandle = pWnd->GetNativeHandle();

    s_vkSurface.Create(vkSurfCreateInfo);
    CORE_ASSERT(s_vkSurface.IsCreated());


    vkn::PhysicalDeviceFeaturesRequirenments vkPhysDeviceFeturesReq = {};
    vkPhysDeviceFeturesReq.independentBlend = true;

    vkn::PhysicalDevicePropertiesRequirenments vkPhysDevicePropsReq = {};
    vkPhysDevicePropsReq.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    vkn::PhysicalDeviceCreateInfo vkPhysDeviceCreateInfo = {};
    vkPhysDeviceCreateInfo.pInstance = &s_vkInstance;
    vkPhysDeviceCreateInfo.pPropertiesRequirenments = &vkPhysDevicePropsReq;
    vkPhysDeviceCreateInfo.pFeaturesRequirenments = &vkPhysDeviceFeturesReq;

    s_vkPhysDevice.Create(vkPhysDeviceCreateInfo);
    CORE_ASSERT(s_vkPhysDevice.IsCreated()); 


    constexpr std::array vkDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceVulkan11Features features11 = {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = &features12;
    features11.shaderDrawParameters = VK_TRUE; // Enables slang internal shader variables like "SV_VertexID" etc.

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features11;

    vkn::DeviceCreateInfo vkDeviceCreateInfo = {};
    vkDeviceCreateInfo.pPhysDevice = &s_vkPhysDevice;
    vkDeviceCreateInfo.pSurface = &s_vkSurface;
    vkDeviceCreateInfo.queuePriority = 1.f;
    vkDeviceCreateInfo.extensions = vkDeviceExtensions;
    vkDeviceCreateInfo.pFeatures2 = &features2;

    s_vkDevice.Create(vkDeviceCreateInfo);
    CORE_ASSERT(s_vkDevice.IsCreated());


    vkn::SwapchainCreateInfo vkSwapchainCreateInfo = {};
    vkSwapchainCreateInfo.pDevice = &s_vkDevice;
    vkSwapchainCreateInfo.pSurface = &s_vkSurface;

    vkSwapchainCreateInfo.width = pWnd->GetWidth();
    vkSwapchainCreateInfo.height = pWnd->GetHeight();

    vkSwapchainCreateInfo.minImageCount = 2;
    vkSwapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    vkSwapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    vkSwapchainCreateInfo.imageArrayLayers = 1u;
    vkSwapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vkSwapchainCreateInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    vkSwapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vkSwapchainCreateInfo.presentMode = VSYNC_ENABLED ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    s_vkSwapchain.Create(vkSwapchainCreateInfo);
    CORE_ASSERT(s_vkSwapchain.IsCreated());


    s_vkCmdPool = CreateVkCmdPool(s_vkDevice.Get(), s_vkDevice.GetQueueFamilyIndex());
    s_vkImmediateSubmitCmdBuffer = AllocateVkCmdBuffer(s_vkDevice.Get(), s_vkCmdPool);

    s_vkDescriptorPool = CreateVkDescriptorPool(s_vkDevice.Get());
    s_vkDescriptorSetLayout = CreateVkDescriptorSetLayout(s_vkDevice.Get());
    s_vkDescriptorSet = CreateVkDescriptorSet(s_vkDevice.Get(), s_vkDescriptorPool, s_vkDescriptorSetLayout);

    s_vkPipelineLayout = CreateVkPipelineLayout(s_vkDevice.Get(), s_vkDescriptorSetLayout);
    s_vkPipeline = CreateVkGraphicsPipeline(s_vkDevice.Get(), s_vkPipelineLayout, "shaders/bin/test.vs.spv", "shaders/bin/test.ps.spv");


    vkn::BufferCreateInfo commonConstBufCreateInfo = {};
    commonConstBufCreateInfo.pDevice = &s_vkDevice;
    commonConstBufCreateInfo.size = sizeof(COMMON_CB_DATA);
    commonConstBufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    commonConstBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    commonConstBufCreateInfo.memAllocFlags = 0;

    s_commonConstBuffer.Create(commonConstBufCreateInfo); 
    CORE_ASSERT(s_commonConstBuffer.IsCreated());
    s_commonConstBuffer.SetDebugName("COMMON_CB");

    {
        COMMON_CB_DATA commonConstBuffer = {};
        commonConstBuffer.color = glm::vec3(1.f, 0.f, 1.f);

        void* pCommonConstBufferData = s_commonConstBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pCommonConstBufferData, &commonConstBuffer, sizeof(commonConstBuffer));
        s_commonConstBuffer.Unmap();
    }

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = s_commonConstBuffer.Get();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(COMMON_CB_DATA);

    VkWriteDescriptorSet descWrite = {};
    descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet = s_vkDescriptorSet;
    descWrite.dstBinding = 0;
    descWrite.dstArrayElement = 0;
    descWrite.descriptorCount = 1;
    descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descWrite.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(s_vkDevice.Get(), 1, &descWrite, 0, nullptr);

    const size_t swapchainImageCount = s_vkSwapchain.GetImageCount();

    s_vkRenderingFinishedSemaphores.resize(swapchainImageCount, VK_NULL_HANDLE);
    s_vkPresentFinishedSemaphores.resize(swapchainImageCount, VK_NULL_HANDLE);
    s_vkRenderingFinishedFences.resize(swapchainImageCount);
    s_vkRenderCmdBuffers.resize(swapchainImageCount, VK_NULL_HANDLE);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_vkRenderingFinishedSemaphores[i] = CreateVkSemaphore();
        s_vkPresentFinishedSemaphores[i] = CreateVkSemaphore();
        s_vkRenderingFinishedFences[i].Create(&s_vkDevice);
        s_vkRenderCmdBuffers[i] = AllocateVkCmdBuffer(s_vkDevice.Get(), s_vkCmdPool);
    }

    s_vkImmediateSubmitFinishedFence.Create(&s_vkDevice);


    vkn::BufferCreateInfo stagingBufCreateInfo = {};
    stagingBufCreateInfo.pDevice = &s_vkDevice;
    stagingBufCreateInfo.size = VERTEX_BUFFER_SIZE_BYTES;
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stagingBufCreateInfo.memAllocFlags = 0;

    vkn::Buffer stagingBuffer(stagingBufCreateInfo);
    CORE_ASSERT(stagingBuffer.IsCreated());
    stagingBuffer.SetDebugName("STAGING_BUFFER");

    {
        void* pVertexBufferData = stagingBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pVertexBufferData, TEST_VERTECIES.data(), TEST_VERTECIES.size() * sizeof(TEST_VERTECIES[0]));
        stagingBuffer.Unmap();
    }


    vkn::BufferCreateInfo vertBufCreateInfo = {};
    vertBufCreateInfo.pDevice = &s_vkDevice;
    vertBufCreateInfo.size = VERTEX_BUFFER_SIZE_BYTES;
    vertBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertBufCreateInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vertBufCreateInfo.memAllocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    s_vertexBuffer.Create(vertBufCreateInfo);
    CORE_ASSERT(s_vertexBuffer.IsCreated());
    s_vertexBuffer.SetDebugName("COMMON_VB");


    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](VkCommandBuffer vkCmdBuffer){
        VkBufferCopy region = {};
        region.size = TEST_VERTECIES.size() * sizeof(TEST_VERTECIES[0]);

        vkCmdCopyBuffer(vkCmdBuffer, stagingBuffer.Get(), s_vertexBuffer.Get(), 1, &region);
    });

    stagingBuffer.Destroy();

    pWnd->SetVisible(true);

    Timer timer;

    while(!pWnd->IsClosed()) {
        timer.Reset();

        pWnd->ProcessEvents();
        
        WndEvent event;
        while(pWnd->PopEvent(event)) {
            ProcessWndEvents(event);
        }

        if (pWnd->IsMinimized()) {
            continue;
        }

        if (s_swapchainRecreateRequired) {
            if (ResizeVkSwapchain(pWnd)) {
                continue;
            }
        }

        RenderScene();

    #ifdef ENG_BUILD_DEBUG
        constexpr const char* BUILD_TYPE_STR = "DEBUG";
    #else
        constexpr const char* BUILD_TYPE_STR = "RELEASE";
    #endif

        const float frameTime = timer.End().GetDuration<float, std::milli>();

        pWnd->SetTitle("%s | %s: %.3f ms (%.1f FPS)", BUILD_TYPE_STR, wndInitInfo.pTitle, frameTime, 1000.f / frameTime);
    }

    VK_CHECK(vkDeviceWaitIdle(s_vkDevice.Get()));

    s_commonConstBuffer.Destroy();
    s_vertexBuffer.Destroy(); 

    s_vkImmediateSubmitFinishedFence.Destroy();
    
    for (size_t i = 0; i < s_vkSwapchain.GetImageCount(); ++i) {
        vkDestroySemaphore(s_vkDevice.Get(), s_vkRenderingFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(s_vkDevice.Get(), s_vkPresentFinishedSemaphores[i], nullptr);
        s_vkRenderingFinishedFences[i].Destroy();
    }

    vkDestroyPipeline(s_vkDevice.Get(), s_vkPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_vkPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_vkDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(s_vkDevice.Get(), s_vkDescriptorPool, nullptr);

    vkDestroyCommandPool(s_vkDevice.Get(), s_vkCmdPool, nullptr);
    
    s_vkSwapchain.Destroy();
    s_vkDevice.Destroy();
    s_vkSurface.Destroy();
    s_vkInstance.Destroy();

    pWnd->Destroy();
    wndSysTerminate();

    return 0;
}