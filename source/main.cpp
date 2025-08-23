#include "core/wnd_system/wnd_system.h"

#include "core/platform/file/file.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#ifdef ENG_OS_WINDOWS
    #include <vulkan/vulkan_win32.h>
#endif

#include <glm/glm.hpp>


namespace fs = std::filesystem;


#define VK_LOG_INFO(FMT, ...)         ENG_LOG_INFO("VULKAN", FMT, __VA_ARGS__)
#define VK_LOG_WARN(FMT, ...)         ENG_LOG_WARN("VULKAN", FMT, __VA_ARGS__)
#define VK_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "VULKAN", FMT, __VA_ARGS__)
#define VK_ASSERT(COND)               VK_ASSERT_MSG(COND, #COND)
#define VK_ASSERT_FAIL(FMT, ...)      VK_ASSERT_MSG(false, FMT, __VA_ARGS__)

#define CORE_LOG_INFO(FMT, ...)         ENG_LOG_INFO("CORE", FMT, __VA_ARGS__)
#define CORE_LOG_WARN(FMT, ...)         ENG_LOG_WARN("CORE", FMT, __VA_ARGS__)
#define CORE_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "CORE", FMT, __VA_ARGS__)
#define CORE_ASSERT(COND)               VK_ASSERT_MSG(COND, #COND)
#define CORE_ASSERT_FAIL(FMT, ...)      VK_ASSERT_MSG(false, FMT, __VA_ARGS__)


#define VK_CHECK(VkCall)                                                                  \
    do {                                                                                  \
        const VkResult _vkCallResult = VkCall;                                            \
        (void)_vkCallResult;                                                              \
        VK_ASSERT_MSG(_vkCallResult == VK_SUCCESS, "%s", string_VkResult(_vkCallResult)); \
    } while(0)


struct Buffer
{
    VkBuffer        vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory  vkMemory = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    size = 0;
};


static VkSurfaceFormatKHR s_swapchainSurfFormat = {}; 

static VkDebugUtilsMessengerEXT s_vkDbgUtilsMessenger = VK_NULL_HANDLE;

static VkInstance s_vkInstance = VK_NULL_HANDLE;
static VkSurfaceKHR s_vkSurface = VK_NULL_HANDLE;

static VkPhysicalDevice s_vkPhysDevice = VK_NULL_HANDLE;
static VkPhysicalDeviceMemoryProperties s_vkPhysDeviceMemProps = {};

static uint32_t s_queueFamilyIndex = UINT32_MAX;
static VkQueue s_vkQueue = VK_NULL_HANDLE;
static VkDevice s_vkDevice = VK_NULL_HANDLE;

static VkSwapchainKHR s_vkSwapchain = VK_NULL_HANDLE;
static std::vector<VkImage> s_swapchainImages;
static std::vector<VkImageView> s_swapchainImageViews;
static VkExtent2D s_swapchainExtent = {};

static VkCommandPool s_vkCmdPool = VK_NULL_HANDLE;
static VkCommandBuffer s_vkCmdBuffer = VK_NULL_HANDLE;

static VkPipelineLayout s_vkPipelineLayout = VK_NULL_HANDLE;
static VkPipeline s_vkPipeline = VK_NULL_HANDLE;

static VkSemaphore s_vkPresentFinishedSemaphore = VK_NULL_HANDLE;
static VkSemaphore s_vkRenderingFinishedSemaphore = VK_NULL_HANDLE;
static VkFence s_vkRenderingFinishedFence = VK_NULL_HANDLE;

static Buffer s_vertexBuffer = {};

static bool s_swapchainRecreateRequired = false;


static constexpr size_t VERTEX_BUFFER_SIZE_F4 = 4096;
static constexpr size_t VERTEX_BUFFER_SIZE_BYTES = VERTEX_BUFFER_SIZE_F4 * sizeof(glm::vec4);


struct TestVertex
{
    glm::vec2 ndc;
    glm::vec2 uv;
    glm::vec4 color;
};

static constexpr std::array<TestVertex, 3> TEST_VERTECIES = {
    TestVertex { glm::vec2(-0.5f, 0.5f), glm::vec2(0.f,  0.f), glm::vec4(1.f, 0.f, 0.f, 1.f) },
    TestVertex { glm::vec2( 0.5f, 0.5f), glm::vec2(1.f,  0.f), glm::vec4(0.f, 1.f, 0.f, 1.f) },
    TestVertex { glm::vec2( 0.f, -0.5f), glm::vec2(0.5f, 1.f), glm::vec4(0.f, 0.f, 1.f, 1.f) },
};


class Timer
{
public:
    Timer()
    {
        Start();
    }

    Timer& Reset()
    { 
        m_start = m_end = std::chrono::high_resolution_clock::now();
        return *this;
    }

    Timer& Start()
    {
        m_start = std::chrono::high_resolution_clock::now();
        return *this;
    }

    Timer& End()
    {
        m_end = std::chrono::high_resolution_clock::now();
        return *this;
    }


    template<typename DURATION_T, typename PERIOD_T>
    DURATION_T GetDuration() const
    {
        CORE_ASSERT_MSG(m_end > m_start, "Need to call End() before GetDuration()");
        return std::chrono::duration<DURATION_T, PERIOD_T>(m_end - m_start).count();
    }

private:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    TimePoint m_start;
    TimePoint m_end;
};


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


    GraphicsPipelineBuilder& AddRenderingColorAttachmentFormat(VkFormat format)
    {
        CORE_ASSERT(m_colorAttachmentFormatsCount + 1 <= MAX_COLOR_ATTACHMENTS_COUNT);
        m_colorAttachmentFormats[m_colorAttachmentFormatsCount++] = format;

        return *this;
    }

    GraphicsPipelineBuilder& AddRenderingColorAttachmentFormat(const std::span<const VkFormat> formats)
    {
        for (VkFormat format : formats) {
            AddRenderingColorAttachmentFormat(format);
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

        CORE_ASSERT(m_colorBlendAttachmentStatesCount == m_colorAttachmentFormatsCount);
        CORE_ASSERT(m_layout != VK_NULL_HANDLE);

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
        dynamicState.pDynamicStates = m_dynamicStateValues.data();
        dynamicState.dynamicStateCount = m_dynamicStatesCount;
        
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

    // std::array<VkVertexInputBindingDescription, MAX_VERTEX_ATTRIBUTES_COUNT> m_vertexInputBindingDescs = {};
    // std::array<VkVertexInputAttributeDescription, MAX_VERTEX_ATTRIBUTES_COUNT> m_vertexInputAttributesDescs = {};
    // size_t m_vertexInputAttribsCount = 0;

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


static bool CheckVkInstExtensionsSupport(const std::span<const char* const> requiredExtensions)
{
    uint32_t vkInstExtensionPropsCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &vkInstExtensionPropsCount, nullptr));
    std::vector<VkExtensionProperties> vkInstExtensionProps(vkInstExtensionPropsCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &vkInstExtensionPropsCount, vkInstExtensionProps.data()));

    for (const char* pExtensionName : requiredExtensions) {
        const auto reqExtIt = std::find_if(vkInstExtensionProps.cbegin(), vkInstExtensionProps.cend(), [&](const VkExtensionProperties& props) {
            return strcmp(pExtensionName, props.extensionName) == 0;
        });
        
        if (reqExtIt == vkInstExtensionProps.cend()) {
            return false;
        }
    }

    return true;
}


static bool CheckVkInstLayersSupport(const std::span<const char* const> requiredLayers)
{
    uint32_t vkInstLayersPropsCount = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&vkInstLayersPropsCount, nullptr));
    std::vector<VkLayerProperties> vkInstLayerProps(vkInstLayersPropsCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&vkInstLayersPropsCount, vkInstLayerProps.data()));

    for (const char* pLayerName : requiredLayers) {
        const auto reqLayerIt = std::find_if(vkInstLayerProps.cbegin(), vkInstLayerProps.cend(), [&](const VkLayerProperties& props) {
            return strcmp(pLayerName, props.layerName) == 0;
        });
        
        if (reqLayerIt == vkInstLayerProps.cend()) {
            return false;
        }
    }

    return true;
}


static bool CheckVkDeviceExtensionsSupport(VkPhysicalDevice vkPhysDevice, const std::span<const char* const> requiredExtensions)
{
    uint32_t vkDeviceExtensionsCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, nullptr));
    std::vector<VkExtensionProperties> vkDeviceExtensionProps(vkDeviceExtensionsCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, vkDeviceExtensionProps.data()));

    for (const char* pExtensionName : requiredExtensions) {
        const auto reqLayerIt = std::find_if(vkDeviceExtensionProps.cbegin(), vkDeviceExtensionProps.cend(), [&](const VkExtensionProperties& props) {
            return strcmp(pExtensionName, props.extensionName) == 0;
        });
        
        if (reqLayerIt == vkDeviceExtensionProps.cend()) {
            return false;
        }
    }

    return true;
}


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


static VkDebugUtilsMessengerEXT CreateVkDebugMessenger(VkInstance vkInstance, const VkDebugUtilsMessengerCreateInfoEXT& vkDbgMessengerCreateInfo)
{
    VkDebugUtilsMessengerEXT vkDbgUtilsMessenger = VK_NULL_HANDLE;

    auto CreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT");
    VK_ASSERT(CreateDebugUtilsMessenger);

    VK_CHECK(CreateDebugUtilsMessenger(vkInstance, &vkDbgMessengerCreateInfo, nullptr, &vkDbgUtilsMessenger));
    VK_ASSERT(vkDbgUtilsMessenger != VK_NULL_HANDLE);

    CreateDebugUtilsMessenger = nullptr;

    return vkDbgUtilsMessenger;
}


static void DestroyVkDebugMessenger(VkInstance vkInstance, VkDebugUtilsMessengerEXT& vkDbgUtilsMessenger)
{
    if (vkDbgUtilsMessenger == VK_NULL_HANDLE) {
        return;
    }

    auto DestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT");
    VK_ASSERT(DestroyDebugUtilsMessenger);

    DestroyDebugUtilsMessenger(vkInstance, vkDbgUtilsMessenger, nullptr);
    vkDbgUtilsMessenger = VK_NULL_HANDLE;

    DestroyDebugUtilsMessenger = nullptr;
}


static VkInstance CreateVkInstance(const char* pAppName, VkDebugUtilsMessengerEXT& vkDbgUtilsMessenger)
{
    Timer timer;

    VkApplicationInfo vkApplicationInfo = {};
    vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkApplicationInfo.pApplicationName = pAppName;
    vkApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vkApplicationInfo.pEngineName = "VkEngine";
    vkApplicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkApplicationInfo.apiVersion = VK_API_VERSION_1_3;

    constexpr std::array vkInstExtensions = {
    #ifdef ENG_BUILD_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef ENG_OS_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #endif
    };

    VK_ASSERT_MSG(CheckVkInstExtensionsSupport(vkInstExtensions), "Not all required instance extensions are supported");

#ifdef ENG_BUILD_DEBUG
     constexpr std::array vkInstLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    
    VK_ASSERT_MSG(CheckVkInstLayersSupport(vkInstLayers), "Not all required instance layers are supported");

    VkDebugUtilsMessengerCreateInfoEXT vkDbgMessengerCreateInfo = {};
    vkDbgMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    vkDbgMessengerCreateInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
    vkDbgMessengerCreateInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vkDbgMessengerCreateInfo.pfnUserCallback = DbgVkMessageCallback;
#endif

    VkInstanceCreateInfo vkInstCreateInfo = {};
    vkInstCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstCreateInfo.pApplicationInfo = &vkApplicationInfo;
    
#ifdef ENG_BUILD_DEBUG
    vkInstCreateInfo.pNext = &vkDbgMessengerCreateInfo;
    vkInstCreateInfo.enabledLayerCount = vkInstLayers.size();
    vkInstCreateInfo.ppEnabledLayerNames = vkInstLayers.data();
#else
    vkInstCreateInfo.enabledLayerCount = 0;
    vkInstCreateInfo.ppEnabledLayerNames = nullptr;
#endif

    vkInstCreateInfo.enabledExtensionCount = vkInstExtensions.size();
    vkInstCreateInfo.ppEnabledExtensionNames = vkInstExtensions.data();

    VkInstance vkInstance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&vkInstCreateInfo, nullptr, &vkInstance));
    VK_ASSERT(vkInstance != VK_NULL_HANDLE);

#ifdef ENG_BUILD_DEBUG
    vkDbgUtilsMessenger = CreateVkDebugMessenger(vkInstance, vkDbgMessengerCreateInfo);
#else
    vkDbgUtilsMessenger = VK_NULL_HANDLE;
#endif

    VK_LOG_INFO("VkInstance initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkInstance;
}


static VkSurfaceKHR CreateVkSurface(VkInstance vkInstance, const BaseWindow& wnd)
{
    Timer timer;

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

#ifdef ENG_OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
    vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
    vkWin32SurfCreateInfo.hwnd = (HWND)wnd.GetNativeHandle();

    VK_CHECK(vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfCreateInfo, nullptr, &vkSurface));
#endif

    VK_ASSERT(vkSurface != VK_NULL_HANDLE);

    VK_LOG_INFO("VkSurface initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkSurface;
}


static VkPhysicalDevice CreateVkPhysDevice(VkInstance vkInstance)
{
    Timer timer;

    uint32_t physDeviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &physDeviceCount, nullptr));
    VK_ASSERT(physDeviceCount > 0);
    
    std::vector<VkPhysicalDevice> physDevices(physDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &physDeviceCount, physDevices.data()));

    VkPhysicalDevice pickedPhysDevice = VK_NULL_HANDLE;

    for (VkPhysicalDevice device : physDevices) {
        bool isDeviceSuitable = true;

        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(device, &features);

        isDeviceSuitable = isDeviceSuitable && features.independentBlend;

        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(device, &props);

        isDeviceSuitable = isDeviceSuitable && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

        if (isDeviceSuitable) {
            pickedPhysDevice = device;
            break;
        }
    }

    VK_ASSERT(pickedPhysDevice != VK_NULL_HANDLE);

    s_vkPhysDeviceMemProps = {};
    vkGetPhysicalDeviceMemoryProperties(pickedPhysDevice, &s_vkPhysDeviceMemProps);

    VK_LOG_INFO("VkPhysicalDevice initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return pickedPhysDevice;
}


static VkDevice CreateVkDevice(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, uint32_t& queueFamilyIndex, VkQueue& vkQueue)
{
    Timer timer;

    uint32_t queueFamilyPropsCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysDevice, &queueFamilyPropsCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropsCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysDevice, &queueFamilyPropsCount, queueFamilyProps.data());

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;
    uint32_t transferQueueFamilyIndex = UINT32_MAX;

    auto IsQueueFamilyIndexValid = [](uint32_t index) -> bool { return index != UINT32_MAX; };

    for (uint32_t i = 0; i < queueFamilyProps.size(); ++i) {
        const VkQueueFamilyProperties& props = queueFamilyProps[i];

        VkBool32 isPresentSupported = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysDevice, i, vkSurface, &isPresentSupported));
        if (!isPresentSupported) {
            continue;
        }

        if (!IsQueueFamilyIndexValid(graphicsQueueFamilyIndex) && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphicsQueueFamilyIndex = i;
        }

        if (!IsQueueFamilyIndexValid(computeQueueFamilyIndex) && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            computeQueueFamilyIndex = i;
        }

        if (!IsQueueFamilyIndexValid(transferQueueFamilyIndex) && (props.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            transferQueueFamilyIndex = i;
        }

        if (IsQueueFamilyIndexValid(graphicsQueueFamilyIndex) && 
            IsQueueFamilyIndexValid(computeQueueFamilyIndex) && 
            IsQueueFamilyIndexValid(transferQueueFamilyIndex)
        ) {
            break;
        }
    }

    VK_ASSERT_MSG(IsQueueFamilyIndexValid(graphicsQueueFamilyIndex), "Failed to get graphics queue family index");
    VK_ASSERT_MSG(IsQueueFamilyIndexValid(computeQueueFamilyIndex), "Failed to get compute queue family index");
    VK_ASSERT_MSG(IsQueueFamilyIndexValid(transferQueueFamilyIndex), "Failed to get transfer queue family index");

    VK_ASSERT_MSG(graphicsQueueFamilyIndex == computeQueueFamilyIndex && computeQueueFamilyIndex == transferQueueFamilyIndex,
        "Queue family indices for graphics, compute and transfer must be equal, for now. TODO: process the case when they are different");

    queueFamilyIndex = graphicsQueueFamilyIndex;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    
    const float queuePriority = 1.f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    constexpr std::array deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VK_ASSERT(CheckVkDeviceExtensionsSupport(vkPhysDevice, deviceExtensions));

    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

    deviceCreateInfo.pNext = &features2;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(vkPhysDevice, &deviceCreateInfo, nullptr, &device));
    VK_ASSERT(device);
    
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &vkQueue);
    VK_ASSERT(vkQueue);

    VK_LOG_INFO("VkDevice initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return device;
}


static bool CheckVkSurfaceFormatSupport(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, VkSurfaceFormatKHR format)
{
    uint32_t surfaceFormatsCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysDevice, vkSurface, &surfaceFormatsCount, nullptr));
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysDevice, vkSurface, &surfaceFormatsCount, surfaceFormats.data()));

    if (surfaceFormatsCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        return true;
    }

    for (VkSurfaceFormatKHR fmt : surfaceFormats) {
        if (fmt.format == format.format && fmt.colorSpace == format.colorSpace) {
            return true;
        }
    }

    return false;
}


static bool CheckVkPresentModeSupport(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, VkPresentModeKHR presentMode)
{
    uint32_t presentModesCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysDevice, vkSurface, &presentModesCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModesCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysDevice, vkSurface, &presentModesCount, presentModes.data()));

    for (VkPresentModeKHR mode : presentModes) {
        if (mode == presentMode) {
            return true;
        }
    }

    return false;
}


static VkSwapchainKHR CreateVkSwapchain(VkPhysicalDevice vkPhysDevice, VkDevice vkDevice, VkSurfaceKHR vkSurface, 
    VkExtent2D requiredExtent, VkSwapchainKHR oldSwapchain, VkExtent2D& swapchainExtent)
{
    Timer timer;

    VkSurfaceCapabilitiesKHR surfCapabilities = {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysDevice, vkSurface, &surfCapabilities));

    VkExtent2D extent = {};
    if (surfCapabilities.currentExtent.width != UINT32_MAX || surfCapabilities.currentExtent.height != UINT32_MAX) {
        extent = surfCapabilities.currentExtent;
    } else {
        extent.width = std::clamp(requiredExtent.width, surfCapabilities.minImageExtent.width, surfCapabilities.maxImageExtent.width);
        extent.height = std::clamp(requiredExtent.height, surfCapabilities.minImageExtent.height, surfCapabilities.maxImageExtent.height);
    }

    if (extent.width == 0 || extent.height == 0) {
        return oldSwapchain;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.oldSwapchain = oldSwapchain;
    swapchainCreateInfo.surface = vkSurface;
    swapchainCreateInfo.imageArrayLayers = 1u;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Since we have one queue for graphics, compute and transfer
    swapchainCreateInfo.imageExtent = extent;

    s_swapchainSurfFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    s_swapchainSurfFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VK_ASSERT_MSG(CheckVkSurfaceFormatSupport(vkPhysDevice, vkSurface, s_swapchainSurfFormat), "Unsupported swapchain surface format");

    swapchainCreateInfo.imageFormat = s_swapchainSurfFormat.format;
    swapchainCreateInfo.imageColorSpace = s_swapchainSurfFormat.colorSpace;

    swapchainCreateInfo.minImageCount = surfCapabilities.minImageCount + 1;
    if (surfCapabilities.maxImageCount != 0) {
        swapchainCreateInfo.minImageCount = std::min(swapchainCreateInfo.minImageCount, surfCapabilities.maxImageCount);
    }

    swapchainCreateInfo.preTransform = (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) 
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCapabilities.currentTransform;

    swapchainCreateInfo.presentMode = CheckVkPresentModeSupport(vkPhysDevice, vkSurface, VK_PRESENT_MODE_MAILBOX_KHR) 
        ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    VK_ASSERT_MSG(CheckVkPresentModeSupport(vkPhysDevice, vkSurface, swapchainCreateInfo.presentMode), "Unsupported swapchain present mode");

    swapchainCreateInfo.clipped = VK_TRUE;

    VK_ASSERT(swapchainCreateInfo.minImageCount >= surfCapabilities.minImageCount);
    if (surfCapabilities.maxImageCount != 0) {
        VK_ASSERT(swapchainCreateInfo.minImageCount <= surfCapabilities.maxImageCount);
    }
    VK_ASSERT((swapchainCreateInfo.compositeAlpha & surfCapabilities.supportedCompositeAlpha) == swapchainCreateInfo.compositeAlpha);
    VK_ASSERT((swapchainCreateInfo.preTransform & surfCapabilities.supportedTransforms) == swapchainCreateInfo.preTransform);
    VK_ASSERT((swapchainCreateInfo.imageUsage & surfCapabilities.supportedUsageFlags) == swapchainCreateInfo.imageUsage);

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(vkDevice, &swapchainCreateInfo, nullptr, &swapchain));
    VK_ASSERT(swapchain);

    if (oldSwapchain) {
        vkDestroySwapchainKHR(vkDevice, oldSwapchain, nullptr);
    }

    swapchainExtent = swapchainCreateInfo.imageExtent;

    VK_LOG_INFO("VkSwapchain initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return swapchain;
}


static void GetVkSwapchainImages(VkDevice vkDevice, VkSwapchainKHR vkSwapchain, std::vector<VkImage>& swapchainImages)
{
    Timer timer;

    uint32_t swapchainImagesCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImagesCount, nullptr));
    swapchainImages.resize(swapchainImagesCount);
    VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImagesCount, swapchainImages.data()));

    VK_LOG_INFO("Getting VkSwapchain Images finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void CreateVkSwapchainImageView(VkDevice vkDevice, const std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews)
{
    if (swapchainImages.empty()) {
        return;
    }

    Timer timer;

    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = s_swapchainSurfFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.levelCount = 1;

        VkImageView& view = swapchainImageViews[i];

        VK_CHECK(vkCreateImageView(vkDevice, &imageViewCreateInfo, nullptr, &view));
    }

    VK_LOG_INFO("VkSwapchain Image Views initializing finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void DestroyVkSwapchainImageViews(VkDevice vkDevice, std::vector<VkImageView>& swapchainImageViews)
{
    Timer timer;

    if (swapchainImageViews.empty()) {
        return;
    }

    for (VkImageView& view : swapchainImageViews) {
        vkDestroyImageView(vkDevice, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    swapchainImageViews.clear();

    VK_LOG_INFO("VkSwapchain Image Views destroying finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static VkSwapchainKHR RecreateVkSwapchain(VkPhysicalDevice vkPhysDevice, VkDevice vkDevice, VkSurfaceKHR vkSurface, VkExtent2D requiredExtent, 
    VkSwapchainKHR oldSwapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkExtent2D& swapchainExtent)
{
    VK_CHECK(vkDeviceWaitIdle(vkDevice));

    VkSwapchainKHR vkSwapchain = CreateVkSwapchain(vkPhysDevice, vkDevice, vkSurface, requiredExtent, oldSwapchain, swapchainExtent);
                
    if (vkSwapchain != VK_NULL_HANDLE && vkSwapchain != oldSwapchain) {
        DestroyVkSwapchainImageViews(vkDevice, swapchainImageViews);

        GetVkSwapchainImages(vkDevice, vkSwapchain, swapchainImages);
        CreateVkSwapchainImageView(vkDevice, swapchainImages, swapchainImageViews);
    }

    return vkSwapchain;
}


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


static VkPipelineLayout CreateVkPipelineLayout(VkDevice vkDevice)
{
    Timer timer;

    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    VkPushConstantRange pushConstants = {};
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstants.size = sizeof(VkDeviceAddress);

    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstants;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(vkDevice, &createInfo, nullptr, &layout));
    VK_ASSERT(layout != VK_NULL_HANDLE);

    VK_LOG_INFO("VkPipelineLayout initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return layout;
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
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetLayout(vkLayout)
        .Build(vkDevice);

    // constexpr VkFormat colorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;

    for (VkShaderModule& shader : vkShaderModules) {
        vkDestroyShaderModule(vkDevice, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    VK_LOG_INFO("VkPipeline (graphics) initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPipeline;
}


VkSemaphore CreateVkSemaphore()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(s_vkDevice, &semaphoreCreateInfo, nullptr, &vkSemaphore));
    VK_ASSERT(vkSemaphore != VK_NULL_HANDLE);

    return vkSemaphore;
}


VkFence CreateVkFence()
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence vkFence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(s_vkDevice, &fenceCreateInfo, nullptr, &vkFence));
    VK_ASSERT(vkFence != VK_NULL_HANDLE);

    return vkFence;
}


static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < s_vkPhysDeviceMemProps.memoryTypeCount; ++i) {
        const VkMemoryPropertyFlags propertyFlags = s_vkPhysDeviceMemProps.memoryTypes[i].propertyFlags;
        
        if ((typeFilter & (1 << i)) && (propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}


static Buffer CreateBuffer(VkDevice vkDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags memAllocFlags)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(vkDevice, &bufferCreateInfo, nullptr, &vkBuffer));
    VK_ASSERT(vkBuffer != VK_NULL_HANDLE);

    VkBufferMemoryRequirementsInfo2 memRequirementsInfo = {};
    memRequirementsInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    memRequirementsInfo.buffer = vkBuffer;

    VkMemoryRequirements2 memRequirements = {};
    memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetBufferMemoryRequirements2(vkDevice, &memRequirementsInfo, &memRequirements);

    VkMemoryAllocateFlagsInfo memAllocFlagsInfo = {};
    memAllocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    memAllocFlagsInfo.flags = memAllocFlags;

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = &memAllocFlagsInfo;
    memAllocInfo.allocationSize = size;
    memAllocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryRequirements.memoryTypeBits, properties);
    VK_ASSERT_MSG(memAllocInfo.memoryTypeIndex != UINT32_MAX, "Failed to find required memory type index");

    VkDeviceMemory vkBufferMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(vkDevice, &memAllocInfo, nullptr, &vkBufferMemory));
    VK_ASSERT(vkBufferMemory != VK_NULL_HANDLE);

    VkBindBufferMemoryInfo bindInfo = {};
    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
    bindInfo.buffer = vkBuffer;
    bindInfo.memory = vkBufferMemory;

    VK_CHECK(vkBindBufferMemory2(vkDevice, 1, &bindInfo));

    Buffer buffer = {};
    buffer.vkBuffer = vkBuffer;
    buffer.vkMemory = vkBufferMemory;
    buffer.size = size;

    VkBufferDeviceAddressInfo addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = vkBuffer;
    buffer.deviceAddress = vkGetBufferDeviceAddress(vkDevice, &addressInfo);

    return buffer;
}


static void DestroyBuffer(VkDevice vkDevice, Buffer& buffer)
{
    vkFreeMemory(vkDevice, buffer.vkMemory, nullptr);
    vkDestroyBuffer(vkDevice, buffer.vkBuffer, nullptr);

    buffer.vkBuffer = VK_NULL_HANDLE;
    buffer.vkMemory = VK_NULL_HANDLE;
    buffer.deviceAddress = 0;
    buffer.size = 0;
}


void CmdPipelineImageBarrier(
    VkCommandBuffer cmdBuffer, 
    VkImageLayout oldLayout, 
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStageMask, 
    VkPipelineStageFlags dstStageMask,
    VkAccessFlags srcAccessMask, 
    VkAccessFlags dstAccessMask,
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


void ProcessWndEvents(const WndEvent& event)
{
    if (event.Is<WndResizeEvent>()) {
        s_swapchainRecreateRequired = true;
    }
}


void RenderScene()
{
    VK_CHECK(vkWaitForFences(s_vkDevice, 1, &s_vkRenderingFinishedFence, VK_TRUE, UINT64_MAX));

    uint32_t nextImageIdx;
    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice, s_vkSwapchain, UINT64_MAX, s_vkPresentFinishedSemaphore, VK_NULL_HANDLE, &nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }

    VK_CHECK(vkResetFences(s_vkDevice, 1, &s_vkRenderingFinishedFence));
    VK_CHECK(vkResetCommandBuffer(s_vkCmdBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImage& rndImage = s_swapchainImages[nextImageIdx];

    VK_CHECK(vkBeginCommandBuffer(s_vkCmdBuffer, &cmdBeginInfo));
        CmdPipelineImageBarrier(
            s_vkCmdBuffer, 
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            rndImage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.extent = s_swapchainExtent;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;

        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = s_swapchainImageViews[nextImageIdx];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = 245.f / 255.f;
        colorAttachment.clearValue.color.float32[1] = 245.f / 255.f;
        colorAttachment.clearValue.color.float32[2] = 220.f / 255.f;
        colorAttachment.clearValue.color.float32[3] = 255.f / 255.f;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(s_vkCmdBuffer, &renderingInfo);
            VkViewport viewport = {};
            viewport.width = s_swapchainExtent.width;
            viewport.height = s_swapchainExtent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(s_vkCmdBuffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.extent = s_swapchainExtent;
            vkCmdSetScissor(s_vkCmdBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(s_vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipeline);

            vkCmdPushConstants(s_vkCmdBuffer, s_vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &s_vertexBuffer.deviceAddress);

            vkCmdDraw(s_vkCmdBuffer, 3, 1, 0, 0);
        vkCmdEndRendering(s_vkCmdBuffer);

        CmdPipelineImageBarrier(
            s_vkCmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
            0,
            rndImage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    VK_CHECK(vkEndCommandBuffer(s_vkCmdBuffer));

    VkSemaphoreSubmitInfo waitSemaphoreInfo = {};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = s_vkPresentFinishedSemaphore;
    waitSemaphoreInfo.value = 0;
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitSemaphoreInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreInfo = {};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = s_vkRenderingFinishedSemaphore;
    signalSemaphoreInfo.value = 0;
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    signalSemaphoreInfo.deviceIndex = 0;
    
    VkCommandBufferSubmitInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfo.commandBuffer = s_vkCmdBuffer;
    commandBufferInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo2 = {};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.waitSemaphoreInfoCount = 1;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &commandBufferInfo;
    submitInfo2.signalSemaphoreInfoCount = 1;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    VK_CHECK(vkQueueSubmit2(s_vkQueue, 1, &submitInfo2, s_vkRenderingFinishedFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &s_vkRenderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &s_vkSwapchain;
    presentInfo.pImageIndices = &nextImageIdx;
    presentInfo.pResults = NULL;
    const VkResult presentResult = vkQueuePresentKHR(s_vkQueue, &presentInfo);

    if (presentResult != VK_SUBOPTIMAL_KHR && presentResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(presentResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }
}


int main(int argc, char* argv[])
{
    wndSysInit();
    BaseWindow* pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = "Vulkan Demo";
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;

    pWnd->Init(wndInitInfo);
    ENG_ASSERT(pWnd->IsInitialized());

    s_vkInstance = CreateVkInstance(wndInitInfo.pTitle, s_vkDbgUtilsMessenger);
    s_vkSurface = CreateVkSurface(s_vkInstance, *pWnd);
    s_vkPhysDevice = CreateVkPhysDevice(s_vkInstance);

    s_vkDevice = CreateVkDevice(s_vkPhysDevice, s_vkSurface, s_queueFamilyIndex, s_vkQueue);

    s_vkSwapchain = VK_NULL_HANDLE; // Assumed that OS will send resize event and swap chain will be created there

    s_vkCmdPool = CreateVkCmdPool(s_vkDevice, s_queueFamilyIndex);
    s_vkCmdBuffer = AllocateVkCmdBuffer(s_vkDevice, s_vkCmdPool);

    s_vkPipelineLayout = CreateVkPipelineLayout(s_vkDevice);
    s_vkPipeline = CreateVkGraphicsPipeline(s_vkDevice, s_vkPipelineLayout, "shaders/bin/test.vert.spv", "shaders/bin/test.frag.spv");

    s_vkPresentFinishedSemaphore = CreateVkSemaphore();
    s_vkRenderingFinishedSemaphore = CreateVkSemaphore();
    s_vkRenderingFinishedFence = CreateVkFence();

    s_vertexBuffer = CreateBuffer(s_vkDevice, VERTEX_BUFFER_SIZE_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    void* pVertexBufferData = nullptr;
    VK_CHECK(vkMapMemory(s_vkDevice, s_vertexBuffer.vkMemory, 0, s_vertexBuffer.size, 0, &pVertexBufferData));
        memcpy(pVertexBufferData, TEST_VERTECIES.data(), TEST_VERTECIES.size() * sizeof(TEST_VERTECIES[0]));
    vkUnmapMemory(s_vkDevice, s_vertexBuffer.vkMemory);

    s_swapchainRecreateRequired = true;

    static auto ResizeSwapchain = [pWnd]() -> void {
        if (!s_swapchainRecreateRequired) {
            return;
        }
        
        const VkSwapchainKHR oldSwapchain = s_vkSwapchain;
        const VkExtent2D requiredExtent = { pWnd->GetWidth(), pWnd->GetHeight() };
        
        s_vkSwapchain = RecreateVkSwapchain(s_vkPhysDevice, s_vkDevice, s_vkSurface, requiredExtent, oldSwapchain, 
            s_swapchainImages, s_swapchainImageViews, s_swapchainExtent);

        s_swapchainRecreateRequired = false;
    };

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
            ResizeSwapchain();
        }

        if (s_vkSwapchain == VK_NULL_HANDLE) {
            continue;
        }

        RenderScene();

        const float frameTime = timer.End().GetDuration<float, std::milli>();
        pWnd->SetTitle("%s: %.3f ms (%.1f FPS)", wndInitInfo.pTitle, frameTime, 1000.f / frameTime);
    }

    VK_CHECK(vkDeviceWaitIdle(s_vkDevice));

    DestroyBuffer(s_vkDevice, s_vertexBuffer);

    vkDestroyFence(s_vkDevice, s_vkRenderingFinishedFence, nullptr);
    vkDestroySemaphore(s_vkDevice, s_vkRenderingFinishedSemaphore, nullptr);
    vkDestroySemaphore(s_vkDevice, s_vkPresentFinishedSemaphore, nullptr);

    vkDestroyPipeline(s_vkDevice, s_vkPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice, s_vkPipelineLayout, nullptr);

    vkDestroyCommandPool(s_vkDevice, s_vkCmdPool, nullptr);

    DestroyVkSwapchainImageViews(s_vkDevice, s_swapchainImageViews);

    vkDestroySwapchainKHR(s_vkDevice, s_vkSwapchain, nullptr);

    vkDestroyDevice(s_vkDevice, nullptr);
    vkDestroySurfaceKHR(s_vkInstance, s_vkSurface, nullptr);
    
    DestroyVkDebugMessenger(s_vkInstance, s_vkDbgUtilsMessenger);
    vkDestroyInstance(s_vkInstance, nullptr);

    pWnd->Destroy();
    wndSysTerminate();

    return 0;
}