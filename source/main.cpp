#include "core/engine/wnd_system/wnd_system.h"

#include "core/platform/file/file.h"
#include "core/utils/timer.h"

#include "render/core/vulkan/vk_instance.h"
#include "render/core/vulkan/vk_surface.h"
#include "render/core/vulkan/vk_phys_device.h"
#include "render/core/vulkan/vk_device.h"
#include "render/core/vulkan/vk_swapchain.h"
#include "render/core/vulkan/vk_fence.h"
#include "render/core/vulkan/vk_semaphore.h"
#include "render/core/vulkan/vk_cmd.h"
#include "render/core/vulkan/vk_buffer.h"
#include "render/core/vulkan/vk_image.h"
#include "render/core/vulkan/vk_pipeline.h"
#include "render/core/vulkan/vk_query.h"

#include "core/engine/camera/camera.h"

#include "core/profiler/cpu_profiler.h"
#include "render/core/vulkan/vk_profiler.h"


#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include <tiny_gltf.h>


namespace fs = std::filesystem;
namespace gltf = tinygltf;

using VertexIndexType = uint32_t;


struct Mesh
{
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t indexCount;
};


struct Vertex
{
    glm::uint posXY;
    glm::uint posZnormX;
    glm::uint normYZ;
    glm::uint texcoord;
};


struct COMMON_CB_DATA
{
    glm::mat4x4 COMMON_VIEW_MATRIX;
    glm::mat4x4 COMMON_PROJ_MATRIX;
    glm::mat4x4 COMMON_VIEW_PROJ_MATRIX;
};


static constexpr size_t MAX_VERTEX_COUNT = 512 * 1024;
static constexpr size_t VERTEX_BUFFER_SIZE_BYTES = MAX_VERTEX_COUNT * sizeof(Vertex);

static constexpr size_t MAX_INDEX_COUNT = 2'000'000;
static constexpr size_t INDEX_BUFFER_SIZE_BYTES = MAX_INDEX_COUNT * sizeof(VertexIndexType);

static constexpr const char* APP_NAME = "Vulkan Demo";

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 10.f;

static BaseWindow* s_pWnd = nullptr;

static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();

static vkn::Device& s_vkDevice = vkn::GetDevice();

static vkn::Profiler& s_vkProfiler = vkn::GetProfiler();

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();

static vkn::CmdPool   s_vkCmdPool;
static vkn::CmdBuffer s_vkImmediateSubmitCmdBuffer;

static VkDescriptorPool      s_vkDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet       s_vkDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_vkDescriptorSetLayout = VK_NULL_HANDLE;

static VkPipelineLayout      s_vkPipelineLayout = VK_NULL_HANDLE;
static VkPipeline            s_vkPipeline = VK_NULL_HANDLE;

static std::vector<vkn::Semaphore> s_vkRenderingFinishedSemaphores;
static vkn::Semaphore s_vkPresentFinishedSemaphore;
static vkn::Fence     s_vkRenderingFinishedFence;
static vkn::CmdBuffer s_vkRenderCmdBuffer;

static vkn::Fence s_vkImmediateSubmitFinishedFence;

static vkn::Image s_vkDepthImage;
static vkn::ImageView s_vkDepthImageView;

static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_indexBuffer;
static vkn::Buffer s_commonConstBuffer;

static vkn::QueryPool s_vkQueryPool;

static std::vector<Mesh> s_meshes;

static eng::Camera s_camera;

static size_t s_frameNumber = 0;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;


#ifdef ENG_VK_DEBUG_UTILS_ENABLED
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


static VkShaderModule CreateVkShaderModule(VkDevice vkDevice, const fs::path& shaderSpirVPath, std::vector<uint8_t>* pExternalBuffer = nullptr)
{
    Timer timer;

    std::vector<uint8_t>* pShaderData = nullptr;
    std::vector<uint8_t> localBuffer;
    
    pShaderData = pExternalBuffer ? pExternalBuffer : &localBuffer;

    const std::string pathS = shaderSpirVPath.string();

    if (!ReadFile(*pShaderData, shaderSpirVPath)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", pathS.c_str());
    }
    VK_ASSERT_MSG(pShaderData->size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", pathS.c_str(), sizeof(uint32_t));

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(pShaderData->data());
    shaderModuleCreateInfo.codeSize = pShaderData->size();

    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &vkShaderModule));
    VK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

    VK_LOG_INFO("Shader module \"%s\" creating finished: %f ms", pathS.c_str(), timer.End().GetDuration<float, std::milli>());

    return vkShaderModule;
}


static VkDescriptorPool CreateVkDescriptorPool(VkDevice vkDevice)
{
    Timer timer;

    vkn::DescriptorPoolBuilder builder;

    VkDescriptorPool vkPool = builder.SetMaxDescriptorSetsCount(5)
        .AddResource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
        .Build(vkDevice);

    VK_LOG_INFO("VkDescriptorPool creating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPool;
}


static VkDescriptorSetLayout CreateVkDescriptorSetLayout(VkDevice vkDevice)
{
    Timer timer;

    vkn::DescriptorSetLayoutBuilder builder;

    VkDescriptorSetLayout vkLayout = builder
        .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
        .Build(vkDevice);

    VK_LOG_INFO("VkDescriptorSetLayout creating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkLayout;
}


static VkDescriptorSet CreateVkDescriptorSet(VkDevice vkDevice, VkDescriptorPool vkDescriptorPool, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    Timer timer;

    vkn::DescriptorSetAllocator allocator;

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

    vkn::PipelineLayoutBuilder plBuilder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

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

    VkPipelineColorBlendAttachmentState blendState = {};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    vkn::GraphicsPipelineBuilder builder;
    
    VkPipeline vkPipeline = builder
        .SetVertexShader(vkShaderModules[0], "main")
        .SetPixelShader(vkShaderModules[1], "main")
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .SetStencilTestState(VK_FALSE, {}, {})
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .SetDepthAttachmentFormat(VK_FORMAT_D32_SFLOAT)
        .AddColorAttachmentFormat(s_vkSwapchain.GetImageFormat())
        .AddColorBlendAttachment(blendState)
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


static void CmdPipelineImageBarrier(
    vkn::CmdBuffer& cmdBuffer, 
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

    cmdBuffer.CmdPipelineBarrier2(vkDependencyInfo);
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
    s_vkImmediateSubmitCmdBuffer.Reset();

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    s_vkImmediateSubmitCmdBuffer.Begin(cmdBeginInfo);
        func(s_vkImmediateSubmitCmdBuffer, std::forward<Args>(args)...);
    s_vkImmediateSubmitCmdBuffer.End();

    SubmitVkQueue(
        vkQueue, 
        s_vkImmediateSubmitCmdBuffer.Get(), 
        s_vkImmediateSubmitFinishedFence.Get(), 
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE,
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE
    );

    s_vkImmediateSubmitFinishedFence.WaitFor(10'000'000'000);
}


void UpdateCommonConstBuffer(BaseWindow* pWnd)
{
    ENG_PROFILE_SCOPED_MARKER_C("UpdateCommonConstBuffer", 200, 200, 0, 255);

    const glm::mat4x4 viewMat = glm::transpose(s_camera.GetViewMatrix());
    
    #ifdef ENG_REVERSED_Z
        glm::mat4x4 projMat = glm::perspective(glm::radians(90.f), (float)pWnd->GetWidth() / pWnd->GetHeight(), 100'000.f, 0.01f);
    #else
        glm::mat4x4 projMat = glm::perspective(glm::radians(90.f), (float)pWnd->GetWidth() / pWnd->GetHeight(), 0.01f, 100'000.f);
    #endif
    projMat[1][1] *= -1.f;
    projMat = glm::transpose(projMat);

    COMMON_CB_DATA* pCommonConstBufferData = static_cast<COMMON_CB_DATA*>(s_commonConstBuffer.Map(0, VK_WHOLE_SIZE, 0));

    pCommonConstBufferData->COMMON_VIEW_MATRIX = viewMat;
    pCommonConstBufferData->COMMON_PROJ_MATRIX = projMat;
    pCommonConstBufferData->COMMON_VIEW_PROJ_MATRIX = viewMat * projMat;

    s_commonConstBuffer.Unmap();
}


void RenderScene()
{
    ENG_PROFILE_SCOPED_MARKER_C("RenderScene", 255, 255, 0, 255);

    vkn::Fence& renderingFinishedFence = s_vkRenderingFinishedFence;

    const VkResult fenceStatus = vkGetFenceStatus(s_vkDevice.Get(), renderingFinishedFence.Get());
    if (fenceStatus == VK_NOT_READY) {
        return;
    } else {
        renderingFinishedFence.Reset();
    }

    UpdateCommonConstBuffer(s_pWnd);

    vkn::Semaphore& presentFinishedSemaphore = s_vkPresentFinishedSemaphore;

    uint32_t nextImageIdx;
    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), 10'000'000'000, presentFinishedSemaphore.Get(), VK_NULL_HANDLE, &nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }

    vkn::Semaphore& renderingFinishedSemaphore = s_vkRenderingFinishedSemaphores[nextImageIdx];
    vkn::CmdBuffer& cmdBuffer = s_vkRenderCmdBuffer;

    cmdBuffer.Reset();

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImage rndImage = s_vkSwapchain.GetImage(nextImageIdx);

    cmdBuffer.Begin(cmdBeginInfo);
        ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(cmdBuffer, "BeginCmdBuffer", 255, 165, 0, 255);

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

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            s_vkDepthImage.Get(),
            VK_IMAGE_ASPECT_DEPTH_BIT
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

        VkRenderingAttachmentInfo depthAttachment = {};
        depthAttachment.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = s_vkDepthImageView.Get();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    #ifdef ENG_REVERSED_Z
        depthAttachment.clearValue.depthStencil.depth = 0.f;
    #else
        depthAttachment.clearValue.depthStencil.depth = 1.f;
    #endif

        renderingInfo.pDepthAttachment = &depthAttachment;

        ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(cmdBuffer, "BeginRender", 200, 120, 0, 255);
        cmdBuffer.BeginRendering(renderingInfo);
            VkViewport viewport = {};
            viewport.width = renderingInfo.renderArea.extent.width;
            viewport.height = renderingInfo.renderArea.extent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            cmdBuffer.CmdSetViewport(0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.extent = renderingInfo.renderArea.extent;
            cmdBuffer.CmdSetScissor(0, 1, &scissor);

            vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipeline);
            
            vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipelineLayout, 0, 1, &s_vkDescriptorSet, 0, nullptr);

            const VkDeviceAddress vertBufferAddress = s_vertexBuffer.GetDeviceAddress();
            vkCmdPushConstants(cmdBuffer.Get(), s_vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &vertBufferAddress);

            vkCmdBindIndexBuffer(cmdBuffer.Get(), s_indexBuffer.Get(), 0, std::is_same_v<VertexIndexType, uint16_t> ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

            for (const Mesh& mesh : s_meshes) {
                cmdBuffer.CmdDrawIndexed(mesh.indexCount, 1, mesh.firstIndex, mesh.firstVertex, 0);
            }
        cmdBuffer.EndRendering();
        ENG_PROFILE_END_GPU_MARKER_SCOPE(cmdBuffer);

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

        ENG_PROFILE_END_GPU_MARKER_SCOPE(cmdBuffer);

        ENG_PROFILE_GPU_COLLECT_STATS(cmdBuffer);
    cmdBuffer.End();

    SubmitVkQueue(
        s_vkDevice.GetQueue(),
        cmdBuffer.Get(),
        renderingFinishedFence.Get(),
        presentFinishedSemaphore.Get(),
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        renderingFinishedSemaphore.Get(),
        VK_PIPELINE_STAGE_2_NONE
    );

    ENG_PROFILE_BEGIN_MARKER_C_SCOPE("Presenting", 200, 200, 0, 255);
    VkSwapchainKHR vkSwapchain = s_vkSwapchain.Get();
    VkSemaphore vkWaitSemaphore = renderingFinishedSemaphore.Get();

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vkWaitSemaphore;
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
    ENG_PROFILE_END_MARKER_SCOPE();
}


static void LoadScene(const fs::path& filepath, vkn::Buffer& vertBuffer, vkn::Buffer& indexBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("LoadScene", 255, 0, 255, 255);

    const std::string pathS = filepath.string();
    CORE_LOG_TRACE("Loading %s...", pathS.c_str());

    gltf::TinyGLTF modelLoader;
    gltf::Model model;
    std::string error, warning;

    const bool isMeshLoaded = filepath.extension() == ".gltf" ? 
        modelLoader.LoadASCIIFromFile(&model, &error, &warning, pathS) :
        modelLoader.LoadBinaryFromFile(&model, &error, &warning, pathS);

    if (!warning.empty()) {
        CORE_LOG_WARN("Warning during %s model loading: %s", pathS.c_str(), warning.c_str());
    }
    CORE_ASSERT_MSG(isMeshLoaded && error.empty(), "Failed to load %s model: %s", pathS.c_str(), error.c_str());

    size_t vertexCount = 0;
    std::for_each(model.meshes.cbegin(), model.meshes.cend(), [&model, &vertexCount](const gltf::Mesh& mesh){
        std::for_each(mesh.primitives.cbegin(), mesh.primitives.cend(), [&model, &vertexCount](const gltf::Primitive& primitive){
            CORE_ASSERT(primitive.attributes.contains("POSITION"));
            const uint32_t positionAccessorIndex = primitive.attributes.at("POSITION");
            const gltf::Accessor& positionAccessor = model.accessors[positionAccessorIndex];

            vertexCount += positionAccessor.count;
        });
    });

    CORE_ASSERT_MSG(vertexCount < MAX_VERTEX_COUNT, "GLTF scene %s vertex buffer overflow: %zu, max vertex count: %zu", pathS.c_str(), vertexCount, MAX_VERTEX_COUNT);

    std::vector<Vertex> cpuVertBuffer;
    cpuVertBuffer.reserve(vertexCount);

    size_t indexCount = 0;
    std::for_each(model.meshes.cbegin(), model.meshes.cend(), [&model, &indexCount](const gltf::Mesh& mesh){
        std::for_each(mesh.primitives.cbegin(), mesh.primitives.cend(), [&model, &indexCount](const gltf::Primitive& primitive){
            if (primitive.indices >= 0) {
                const gltf::Accessor& indexAccessor = model.accessors[primitive.indices];

                indexCount += indexAccessor.count;
            }
        });
    });

    CORE_ASSERT_MSG(indexCount < MAX_INDEX_COUNT, "GLTF scene %s index buffer overflow: %zu, max index count: %zu", pathS.c_str(), indexCount, MAX_INDEX_COUNT);

    std::vector<VertexIndexType> cpuIndexBuffer;
    cpuIndexBuffer.reserve(indexCount);

    s_meshes.reserve(model.meshes.size());
    s_meshes.resize(0);

    for (const gltf::Mesh& mesh : model.meshes) {
        Mesh internalMesh = {};

        internalMesh.firstVertex = cpuVertBuffer.size();
        internalMesh.firstIndex = cpuIndexBuffer.size();

        for (const gltf::Primitive& primitive : mesh.primitives) {
            const VertexIndexType primitiveStartIndex = cpuVertBuffer.size();

            CORE_ASSERT(primitive.attributes.contains("POSITION"));
            const uint32_t positionAccessorIndex = primitive.attributes.at("POSITION");
            const gltf::Accessor& positionAccessor  = model.accessors[positionAccessorIndex];
            
            const auto& positionBufferView = model.bufferViews[positionAccessor.bufferView];
            const auto& positionBuffer = model.buffers[positionBufferView.buffer];

            const uint8_t* pPositionData = positionBuffer.data.data() + positionBufferView.byteOffset + positionAccessor.byteOffset;

            CORE_ASSERT(primitive.attributes.contains("NORMAL"));
            const uint32_t normalAccessorIndex = primitive.attributes.at("NORMAL");
            const gltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];

            const auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];
            const auto& normalBuffer = model.buffers[normalBufferView.buffer];

            const uint8_t* pNormalData = normalBuffer.data.data() + normalBufferView.byteOffset + normalAccessor.byteOffset;

            CORE_ASSERT(primitive.attributes.contains("TEXCOORD_0"));
            const uint32_t texcoordAccessorIndex = primitive.attributes.at("TEXCOORD_0");
            const gltf::Accessor& texcoordAccessor = model.accessors[texcoordAccessorIndex];

            const auto& texcoordBufferView = model.bufferViews[texcoordAccessor.bufferView];
            const auto& texcoordBuffer = model.buffers[texcoordBufferView.buffer];

            const uint8_t* pTexcoordData = texcoordBuffer.data.data() + texcoordBufferView.byteOffset + texcoordAccessor.byteOffset;

            internalMesh.vertexCount += positionAccessor.count;

            for (size_t i = 0; i < positionAccessor.count; ++i) {
                const float* pPosition = reinterpret_cast<const float*>(pPositionData + i * positionAccessor.ByteStride(positionBufferView));
                const float* pNormal = reinterpret_cast<const float*>(pNormalData + i * normalAccessor.ByteStride(normalBufferView));
                const float* pTexcoord = reinterpret_cast<const float*>(pTexcoordData + i * texcoordAccessor.ByteStride(texcoordBufferView));
                
                Vertex vertex = {};

                vertex.posXY = glm::packHalf2x16(glm::vec2(pPosition[0], pPosition[1]));
                vertex.posZnormX = glm::packHalf2x16(glm::vec2(pPosition[2], pNormal[0]));
                vertex.normYZ = glm::packHalf2x16(glm::vec2(pNormal[1], pNormal[2]));
                vertex.texcoord = glm::packHalf2x16(glm::vec2(pTexcoord[0], pTexcoord[1]));

                cpuVertBuffer.emplace_back(vertex);
            }

            CORE_ASSERT_MSG(primitive.indices >= 0, "GLTF primitive must have index accessor");

            const gltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const gltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const gltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

            const uint8_t* pIndexData = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;

            internalMesh.indexCount += indexAccessor.count;

            for (size_t i = 0; i < indexAccessor.count; ++i) {
                uint32_t index = UINT32_MAX;

                switch (indexAccessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        index = static_cast<uint32_t>(*(reinterpret_cast<const uint8_t*>(pIndexData + i)));
                        break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        index = static_cast<uint32_t>(*(reinterpret_cast<const uint16_t*>(pIndexData + i * 2)));
                        break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        index = static_cast<uint32_t>(*(reinterpret_cast<const uint32_t*>(pIndexData + i * 4)));
                        break;
                    default:
                        CORE_ASSERT_FAIL("Invalid GLTF index type: %d", indexAccessor.componentType);
                        break;
                }

                index = primitiveStartIndex + index;

                CORE_ASSERT_MSG(index < std::numeric_limits<VertexIndexType>::max(), "Vertex index is greater than %zu", std::numeric_limits<VertexIndexType>::max());
                cpuIndexBuffer.push_back(static_cast<VertexIndexType>(index));
            }
        }

        s_meshes.emplace_back(internalMesh);
    }


    vkn::BufferCreateInfo stagingBufCreateInfo = {};
    stagingBufCreateInfo.pDevice = &s_vkDevice;
    stagingBufCreateInfo.size = cpuVertBuffer.size() * sizeof(Vertex);
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stagingBufCreateInfo.memAllocFlags = 0;

    vkn::Buffer stagingVertBuffer(stagingBufCreateInfo);
    CORE_ASSERT(stagingVertBuffer.IsCreated());
    stagingVertBuffer.SetDebugName("STAGING_VERT_BUFFER");

    stagingBufCreateInfo.size = cpuIndexBuffer.size() * sizeof(VertexIndexType);

    vkn::Buffer stagingIndexBuffer(stagingBufCreateInfo);
    CORE_ASSERT(stagingIndexBuffer.IsCreated());
    stagingIndexBuffer.SetDebugName("STAGING_IDX_BUFFER");

    {
        void* pVertexBufferData = stagingVertBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pVertexBufferData, cpuVertBuffer.data(), cpuVertBuffer.size() * sizeof(cpuVertBuffer[0]));
        stagingVertBuffer.Unmap();
    }

    {
        void* pIndexBufferData = stagingIndexBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pIndexBufferData, cpuIndexBuffer.data(), cpuIndexBuffer.size() * sizeof(cpuIndexBuffer[0]));
        stagingIndexBuffer.Unmap();
    }

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy region = {};
        
        region.size = cpuVertBuffer.size() * sizeof(cpuVertBuffer[0]);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingVertBuffer.Get(), vertBuffer.Get(), 1, &region);

        region.size = cpuIndexBuffer.size() * sizeof(cpuIndexBuffer[0]);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingIndexBuffer.Get(), indexBuffer.Get(), 1, &region);
    });

    stagingVertBuffer.Destroy();
    stagingIndexBuffer.Destroy();
}


static void CreateDepthImage(vkn::Image& depthImage, vkn::ImageView& depthImageView)
{
    if (depthImageView.IsCreated()) {
        depthImageView.Destroy();
    }

    if (depthImage.IsCreated()) {
        depthImage.Destroy();
    }

    vkn::ImageCreateInfo depthImageCreateInfo = {};
    depthImageCreateInfo.pDevice = &s_vkDevice;

    depthImageCreateInfo.type = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.extent = VkExtent3D{s_pWnd->GetWidth(), s_pWnd->GetHeight(), 1};
    depthImageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; 
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    
    depthImageCreateInfo.memAllocInfo.flags = 0;
    depthImageCreateInfo.memAllocInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    depthImage.Create(depthImageCreateInfo);
    CORE_ASSERT(depthImage.IsCreated());
    depthImage.SetDebugName("COMMON_DEPTH");

    vkn::ImageViewCreateInfo depthImageViewCreateInfo = {};
    depthImageViewCreateInfo.pOwner = &depthImage;
    depthImageViewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    depthImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    depthImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    depthImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthImageViewCreateInfo.subresourceRange.levelCount = 1;
    depthImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;

    depthImageView.Create(depthImageViewCreateInfo);
    CORE_ASSERT(depthImageView.IsValid());
    depthImageView.SetDebugName("COMMON_DEPTH_VIEW");
}


void UpdateTimings(BaseWindow* pWnd, Timer& cpuTimer)
{
#if defined(ENG_BUILD_DEBUG)
    constexpr const char* BUILD_TYPE_STR = "DEBUG";
#elif defined(ENG_BUILD_PROFILE)
    constexpr const char* BUILD_TYPE_STR = "PROFILE";
#else
    constexpr const char* BUILD_TYPE_STR = "RELEASE";
#endif
        
    const float cpuFrameTime = cpuTimer.End().GetDuration<float, std::milli>();

    pWnd->SetTitle("%s | %s | CPU: %.3f ms (%.1f FPS) | Fly Camera Mode (F5): %s", 
        APP_NAME, BUILD_TYPE_STR, cpuFrameTime, 1000.f / cpuFrameTime, s_flyCameraMode ? "ON" : "OFF");
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


static void CameraProcessWndEvent(eng::Camera& camera, const WndEvent& event)
{
    static bool firstEvent = true;

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.IsPressed() || keyEvent.IsHold()) {
            if (keyEvent.key == WndKey::KEY_W) { 
                camera.velocity.z = -CAMERA_SPEED;
            }
            if (keyEvent.key == WndKey::KEY_S) {
                camera.velocity.z = CAMERA_SPEED;
            }
            if (keyEvent.key == WndKey::KEY_A) {
                camera.velocity.x = -CAMERA_SPEED;
            }
            if (keyEvent.key == WndKey::KEY_D) {
                camera.velocity.x = CAMERA_SPEED;
            }
            if (keyEvent.key == WndKey::KEY_E) {
                camera.velocity.y = CAMERA_SPEED;
            }
            if (keyEvent.key == WndKey::KEY_Q) {
                camera.velocity.y = -CAMERA_SPEED;
            }
            if (keyEvent.key == WndKey::KEY_F5) {
                firstEvent = true;
            }
        }

        if (keyEvent.IsReleased()) {
            if (keyEvent.key == WndKey::KEY_W) {
                camera.velocity.z = 0;
            }
            if (keyEvent.key == WndKey::KEY_S) {
                camera.velocity.z = 0;
            }
            if (keyEvent.key == WndKey::KEY_A) {
                camera.velocity.x = 0;
            }
            if (keyEvent.key == WndKey::KEY_D) {
                camera.velocity.x = 0;
            }
            if (keyEvent.key == WndKey::KEY_E) {
                camera.velocity.y = 0;
            }
            if (keyEvent.key == WndKey::KEY_Q) {
                camera.velocity.y = 0;
            }
        }
    }

    if (event.Is<WndCursorEvent>()) {
        const WndCursorEvent& cursorEvent = event.Get<WndCursorEvent>();

        static int16_t prevX = 0;
        static int16_t prevY = 0; 

        if (firstEvent) {
            firstEvent = false;
        } else {
            camera.yaw += (float)(cursorEvent.x - prevX) / 200.f;
            camera.pitch -= (float)(cursorEvent.y - prevY) / 200.f;
        }

        prevX = cursorEvent.x;
        prevY = cursorEvent.y;
    }
}


void ProcessWndEvents(const WndEvent& event)
{
    if (event.Is<WndResizeEvent>()) {
        s_swapchainRecreateRequired = true;
    }

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.key == WndKey::KEY_F5 && keyEvent.IsPressed()) {
            s_flyCameraMode = !s_flyCameraMode;
            
            ShowCursor(!s_flyCameraMode);
        }
    }

    if (s_flyCameraMode) {
        CameraProcessWndEvent(s_camera, event);
    }
}


void ProcessFrame()
{
    ENG_PROFILE_BEGIN_FRAME("Frame");

    Timer timer;

    s_pWnd->ProcessEvents();
    
    WndEvent event;
    while(s_pWnd->PopEvent(event)) {
        ProcessWndEvents(event);
    }

    if (s_pWnd->IsMinimized()) {
        return;
    }

    if (s_swapchainRecreateRequired) {
        if (ResizeVkSwapchain(s_pWnd)) {
            return;
        }

        s_vkDevice.WaitIdle();
        CreateDepthImage(s_vkDepthImage, s_vkDepthImageView);
    }

    s_camera.Update();

    RenderScene();

    UpdateTimings(s_pWnd, timer);

    ++s_frameNumber;

    ENG_PROFILE_END_FRAME("Frame");
}


int main(int argc, char* argv[])
{
    s_camera.velocity = glm::vec3(0.f);
	s_camera.position = glm::vec3(0.f, 0.f, 2.f);
    s_camera.pitch = 0.f;
    s_camera.yaw = 0.f;

    wndSysInit();
    s_pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = APP_NAME;
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;
    wndInitInfo.isVisible = false;

    s_pWnd->Create(wndInitInfo);
    ENG_ASSERT(s_pWnd->IsInitialized());

#ifdef ENG_VK_DEBUG_UTILS_ENABLED
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
    #ifdef ENG_VK_DEBUG_UTILS_ENABLED
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
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    vkInstCreateInfo.layers = vkInstLayers;
    vkInstCreateInfo.pDbgMessengerCreateInfo = &vkDbgMessengerCreateInfo;
#endif

    s_vkInstance.Create(vkInstCreateInfo); 
    CORE_ASSERT(s_vkInstance.IsCreated()); 
    

    vkn::SurfaceCreateInfo vkSurfCreateInfo = {};
    vkSurfCreateInfo.pInstance = &s_vkInstance;
    vkSurfCreateInfo.pWndHandle = s_pWnd->GetNativeHandle();

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


    s_vkProfiler.Create(&s_vkDevice);
    CORE_ASSERT(s_vkProfiler.IsCreated());


    vkn::SwapchainCreateInfo vkSwapchainCreateInfo = {};
    vkSwapchainCreateInfo.pDevice = &s_vkDevice;
    vkSwapchainCreateInfo.pSurface = &s_vkSurface;

    vkSwapchainCreateInfo.width = s_pWnd->GetWidth();
    vkSwapchainCreateInfo.height = s_pWnd->GetHeight();

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


    vkn::CmdPoolCreateInfo vkCmdPoolCreateInfo = {};
    vkCmdPoolCreateInfo.pDevice = &s_vkDevice;
    vkCmdPoolCreateInfo.queueFamilyIndex = s_vkDevice.GetQueueFamilyIndex();
    vkCmdPoolCreateInfo.flags =  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    s_vkCmdPool.Create(vkCmdPoolCreateInfo);
    CORE_ASSERT(s_vkCmdPool.IsCreated());
    s_vkCmdPool.SetDebugName("COMMON_CMD_POOL");
    
    s_vkImmediateSubmitCmdBuffer = s_vkCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CORE_ASSERT(s_vkImmediateSubmitCmdBuffer.IsCreated());
    s_vkImmediateSubmitCmdBuffer.SetDebugName("IMMEDIATE_CMD_BUFFER");


    s_vkImmediateSubmitFinishedFence.Create(&s_vkDevice);


    vkn::QueryCreateInfo queryCreateInfo = {};
    queryCreateInfo.pDevice = &s_vkDevice;
    queryCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryCreateInfo.queryCount = 128;

    s_vkQueryPool.Create(queryCreateInfo);
    CORE_ASSERT(s_vkQueryPool.IsCreated());
    s_vkQueryPool.SetDebugName("COMMON_GPU_QUERY_POOL");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdResetQueryPool(s_vkQueryPool);
    });


    s_vkDescriptorPool = CreateVkDescriptorPool(s_vkDevice.Get());
    s_vkDescriptorSetLayout = CreateVkDescriptorSetLayout(s_vkDevice.Get());
    s_vkDescriptorSet = CreateVkDescriptorSet(s_vkDevice.Get(), s_vkDescriptorPool, s_vkDescriptorSetLayout);

    s_vkPipelineLayout = CreateVkPipelineLayout(s_vkDevice.Get(), s_vkDescriptorSetLayout);
    s_vkPipeline = CreateVkGraphicsPipeline(s_vkDevice.Get(), s_vkPipelineLayout, "shaders/bin/test.vs.spv", "shaders/bin/test.ps.spv");

    const size_t swapchainImageCount = s_vkSwapchain.GetImageCount();

    s_vkRenderingFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_vkRenderingFinishedSemaphores[i].Create(&s_vkDevice);
        CORE_ASSERT(s_vkRenderingFinishedSemaphores[i].IsCreated());

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        char vkObjDbgName[64] = {'\0'};

        sprintf_s(vkObjDbgName, "RND_FINISH_SEMAPHORE_%zu", i);
        s_vkRenderingFinishedSemaphores[i].SetDebugName(vkObjDbgName);
    #endif
    }

    s_vkPresentFinishedSemaphore.Create(&s_vkDevice);
    CORE_ASSERT(s_vkPresentFinishedSemaphore.IsCreated());
    s_vkPresentFinishedSemaphore.SetDebugName("PRESENT_FINISH_SEMAPHORE");

    s_vkRenderingFinishedFence.Create(&s_vkDevice);
    CORE_ASSERT(s_vkRenderingFinishedFence.IsCreated());
    s_vkRenderingFinishedFence.SetDebugName("RND_FINISH_FENCE");
    
    s_vkRenderCmdBuffer = s_vkCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CORE_ASSERT(s_vkRenderCmdBuffer.IsCreated());
    s_vkRenderCmdBuffer.SetDebugName("RND_CMD_BUFFER");

    CreateDepthImage(s_vkDepthImage, s_vkDepthImageView);


    vkn::BufferCreateInfo vertBufCreateInfo = {};
    vertBufCreateInfo.pDevice = &s_vkDevice;
    vertBufCreateInfo.size = VERTEX_BUFFER_SIZE_BYTES;
    vertBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertBufCreateInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vertBufCreateInfo.memAllocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    s_vertexBuffer.Create(vertBufCreateInfo);
    CORE_ASSERT(s_vertexBuffer.IsCreated());
    s_vertexBuffer.SetDebugName("COMMON_VB");


    vkn::BufferCreateInfo idxBufCreateInfo = {};
    idxBufCreateInfo.pDevice = &s_vkDevice;
    idxBufCreateInfo.size = INDEX_BUFFER_SIZE_BYTES;
    idxBufCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    idxBufCreateInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    idxBufCreateInfo.memAllocFlags = 0;

    s_indexBuffer.Create(idxBufCreateInfo);
    CORE_ASSERT(s_indexBuffer.IsCreated());
    s_indexBuffer.SetDebugName("COMMON_IB");

    const fs::path filepath = argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf";
    LoadScene(filepath, s_vertexBuffer, s_indexBuffer);

    vkn::BufferCreateInfo commonConstBufCreateInfo = {};
    commonConstBufCreateInfo.pDevice = &s_vkDevice;
    commonConstBufCreateInfo.size = sizeof(COMMON_CB_DATA);
    commonConstBufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    commonConstBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    commonConstBufCreateInfo.memAllocFlags = 0;

    s_commonConstBuffer.Create(commonConstBufCreateInfo); 
    CORE_ASSERT(s_commonConstBuffer.IsCreated());
    s_commonConstBuffer.SetDebugName("COMMON_CB");

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

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    s_vkQueryPool.Destroy();

    s_commonConstBuffer.Destroy();
    s_indexBuffer.Destroy();
    s_vertexBuffer.Destroy();

    s_vkDepthImageView.Destroy();
    s_vkDepthImage.Destroy();

    s_vkImmediateSubmitFinishedFence.Destroy();
    
    for (size_t i = 0; i < s_vkSwapchain.GetImageCount(); ++i) {
        s_vkRenderingFinishedSemaphores[i].Destroy();
    }

    s_vkPresentFinishedSemaphore.Destroy();
    s_vkRenderingFinishedFence.Destroy();

    vkDestroyPipeline(s_vkDevice.Get(), s_vkPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_vkPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_vkDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(s_vkDevice.Get(), s_vkDescriptorPool, nullptr);

    s_vkCmdPool.Destroy();
    
    s_vkSwapchain.Destroy();

    s_vkProfiler.Destroy();
    s_vkDevice.Destroy();
    s_vkSurface.Destroy();
    s_vkInstance.Destroy();

    s_pWnd->Destroy();
    wndSysTerminate();

    return 0;
}