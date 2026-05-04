#include "pch.h"

#include "vk_device.h"
#include "vk_cmd.h"
#include "vk_fence.h"
#include "vk_semaphore.h"
#include "vk_swapchain.h"


namespace vkn
{
    static void CheckDeviceExtensionsSupport(VkPhysicalDevice vkPhysDevice, const std::span<const char* const> requiredExtensions)
    {
    #ifdef ENG_VK_DEBUG_UTILS_ENABLED
        uint32_t vkDeviceExtensionsCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, nullptr));
        std::vector<VkExtensionProperties> vkDeviceExtensionProps(vkDeviceExtensionsCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, vkDeviceExtensionProps.data()));

        for (const char* pExtensionName : requiredExtensions) {
            const auto reqLayerIt = std::find_if(vkDeviceExtensionProps.cbegin(), vkDeviceExtensionProps.cend(), [&](const VkExtensionProperties& props) {
                return strcmp(pExtensionName, props.extensionName) == 0;
            });
            
            VK_ASSERT_MSG(reqLayerIt != vkDeviceExtensionProps.cend(), "\'%s\' device extension is not supported", pExtensionName);
        }
    #else
        (void)vkPhysDevice;
        (void)requiredExtensions;
    #endif
    }


    Queue::~Queue()
    {
        Destroy();
    }


    Queue& Queue::Submit(std::span<CmdBuffer*> cmdBuffers, Fence* pFinishFence, 
        std::span<QueueSyncData> waitSemaphores,
        std::span<QueueSyncData> signalSemaphores
    ) {
        VK_ASSERT(IsCreated());
        VK_ASSERT(!cmdBuffers.empty());
        
        m_waitSemaphoreCache.resize(waitSemaphores.size());
        for (size_t i = 0; i < waitSemaphores.size(); ++i) {
            const QueueSyncData& data = waitSemaphores[i];
            VK_ASSERT(data.pSemaphore && data.pSemaphore->IsCreated());

            VkSemaphoreSubmitInfo& waitSemaphoreInfo = m_waitSemaphoreCache[i];
            waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            waitSemaphoreInfo.semaphore = data.pSemaphore->Get();
            waitSemaphoreInfo.value = 0;
            waitSemaphoreInfo.stageMask = data.stage;
            waitSemaphoreInfo.deviceIndex = 0;
        }

        m_signalSemaphoreCache.resize(signalSemaphores.size());
        for (size_t i = 0; i < signalSemaphores.size(); ++i) {
            const QueueSyncData& data = signalSemaphores[i];
            VK_ASSERT(data.pSemaphore && data.pSemaphore->IsCreated());

            VkSemaphoreSubmitInfo& signalSemaphoreInfo = m_signalSemaphoreCache[i];
            signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signalSemaphoreInfo.semaphore = data.pSemaphore->Get();
            signalSemaphoreInfo.value = 0;
            signalSemaphoreInfo.stageMask = data.stage;
            signalSemaphoreInfo.deviceIndex = 0;
        }

        m_cmdBuffCache.resize(cmdBuffers.size());
        for (size_t i = 0; i < cmdBuffers.size(); ++i) {
            CmdBuffer* pCmdBuffer = cmdBuffers[i];
            VK_ASSERT(pCmdBuffer && pCmdBuffer->IsCreated());
            VK_ASSERT(!pCmdBuffer->IsStarted());

            VkCommandBufferSubmitInfo& bufferSubmitInfo = m_cmdBuffCache[i];
            bufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            bufferSubmitInfo.commandBuffer = pCmdBuffer->Get();
            bufferSubmitInfo.deviceMask = 0;
        }

        VkSubmitInfo2 submitInfo2 = {};
        submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo2.commandBufferInfoCount   = m_cmdBuffCache.size();
        submitInfo2.pCommandBufferInfos      = m_cmdBuffCache.empty() ? nullptr : m_cmdBuffCache.data();
        submitInfo2.waitSemaphoreInfoCount   = m_waitSemaphoreCache.size();
        submitInfo2.pWaitSemaphoreInfos      = m_waitSemaphoreCache.empty() ? nullptr : m_waitSemaphoreCache.data();
        submitInfo2.signalSemaphoreInfoCount = m_signalSemaphoreCache.size();
        submitInfo2.pSignalSemaphoreInfos    = m_signalSemaphoreCache.empty() ? nullptr : m_signalSemaphoreCache.data();
        
        VK_CHECK(vkQueueSubmit2(Get(), 1, &submitInfo2, pFinishFence ? pFinishFence->Get() : VK_NULL_HANDLE));
    
        return *this;
    }


    Queue& Queue::Submit(CmdBuffer& cmdBuffer, Fence* pFinishFence, QueueSyncData* pWaitSemaphore, QueueSyncData* pSignalSemaphore)
    {
        CmdBuffer* pCmdBuffer = &cmdBuffer;
        
        return Submit(std::span(&pCmdBuffer, 1), pFinishFence, 
            std::span(pWaitSemaphore, pWaitSemaphore ? 1 : 0), 
            std::span(pSignalSemaphore, pSignalSemaphore ? 1 : 0)
        );
    }


    VkResult Queue::Present(Swapchain& swapchain, uint32_t imageIndex, Semaphore* pWaitSemaphores)
    {
        return Present(swapchain, imageIndex, std::span(&pWaitSemaphores, pWaitSemaphores ? 1 : 0));
    }


    VkResult Queue::Present(Swapchain& swapchain, uint32_t imageIndex, std::span<Semaphore*> waitSemaphores)
    {
        VK_ASSERT(IsCreated());

        m_presentSemaphoreCache.resize(waitSemaphores.size());
        for (size_t i = 0; i < waitSemaphores.size(); ++i) {
            VK_ASSERT(waitSemaphores[i] != nullptr);
            m_presentSemaphoreCache[i] = waitSemaphores[i]->Get();
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = m_presentSemaphoreCache.size();
        presentInfo.pWaitSemaphores = m_presentSemaphoreCache.empty() ? nullptr : m_presentSemaphoreCache.data();
        presentInfo.swapchainCount = 1;
        
        VkSwapchainKHR vkSwapchain = swapchain.Get();
        presentInfo.pSwapchains = &vkSwapchain;
        
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        return vkQueuePresentKHR(Get(), &presentInfo);
    }


    Device& Queue::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pOwner;
    }


    uint32_t Queue::GetFamilyIndex() const
    {
        VK_ASSERT(IsCreated());
        return m_familyIndex;
    }


    Queue::Queue(Device* pOwner, VkQueue queue, uint32_t familyIndex)
    {
        Create(pOwner, queue, familyIndex);
    }


    Queue::Queue(Queue&& queue) noexcept
    {
        *this = std::move(queue);
    }


    Queue& Queue::operator=(Queue&& queue) noexcept
    {
        if (this == &queue) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pOwner, queue.m_pOwner);
        std::swap(m_familyIndex, queue.m_familyIndex);
        std::swap(m_presentSemaphoreCache, queue.m_presentSemaphoreCache);
        std::swap(m_cmdBuffCache, queue.m_cmdBuffCache);
        std::swap(m_waitSemaphoreCache, queue.m_waitSemaphoreCache);
        std::swap(m_signalSemaphoreCache, queue.m_signalSemaphoreCache);

        Base::operator=(std::move(queue));

        return *this;
    }


    Queue& Queue::Create(Device* pOwner, VkQueue queue, uint32_t familyIndex)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of queue %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(pOwner);
        VK_ASSERT(queue != VK_NULL_HANDLE);

        m_pOwner = pOwner;
        m_familyIndex = familyIndex;

        Base::Create([vkQueue = queue](VkQueue& queue) {
            queue = vkQueue;
            return queue != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

        return *this;
    }


    Queue& Queue::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        m_familyIndex = UINT32_MAX;
        m_pOwner = nullptr;
        m_presentSemaphoreCache = {};
        m_cmdBuffCache = {};
        m_waitSemaphoreCache = {};
        m_signalSemaphoreCache = {};

        Base::Destroy([](VkQueue& queue) {
            queue = VK_NULL_HANDLE;
        });

        return *this;
    }


    Device::~Device()
    {
        Destroy();
    }


    Device& Device::Create(const DeviceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of Vulkan device");
            Destroy();
        }

        VK_ASSERT(info.pPhysDevice && info.pPhysDevice->IsCreated());

        if (info.pSurface) {
            VK_ASSERT(info.pSurface->IsCreated());
        }

        CheckDeviceExtensionsSupport(info.pPhysDevice->Get(), info.extensions);

        m_pPhysDevice = info.pPhysDevice;

        uint32_t queueFamilyPropsCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_pPhysDevice->Get(), &queueFamilyPropsCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropsCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_pPhysDevice->Get(), &queueFamilyPropsCount, queueFamilyProps.data());

        uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
        uint32_t computeQueueFamilyIndex = UINT32_MAX;
        uint32_t transferQueueFamilyIndex = UINT32_MAX;

        auto IsQueueFamilyIndexValid = [](uint32_t index) -> bool { return index != UINT32_MAX; };

        for (uint32_t i = 0; i < queueFamilyProps.size(); ++i) {
            const VkQueueFamilyProperties& props = queueFamilyProps[i];

            if (info.pSurface) {
                VkBool32 isPresentSupported = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_pPhysDevice->Get(), i, info.pSurface->Get(), &isPresentSupported));
                
                if (!isPresentSupported) {
                    continue;
                }
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
        VK_ASSERT_MSG(IsQueueFamilyIndexValid(computeQueueFamilyIndex),  "Failed to get compute queue family index");
        VK_ASSERT_MSG(IsQueueFamilyIndexValid(transferQueueFamilyIndex), "Failed to get transfer queue family index");

        VK_ASSERT_MSG(graphicsQueueFamilyIndex == computeQueueFamilyIndex && computeQueueFamilyIndex == transferQueueFamilyIndex,
            "Queue family indices for graphics, compute and transfer must be equal, for now. TODO: process the case when they are different");

        const uint32_t queueFamilyIndex = graphicsQueueFamilyIndex;

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &info.queuePriority;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = info.pFeatures2;
        deviceCreateInfo.pEnabledFeatures = info.pFeatures;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = info.extensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = info.extensions.empty() ? nullptr : info.extensions.data();

        Base::Create([pPhysDevice = m_pPhysDevice, &deviceCreateInfo](VkDevice& device) {
            VK_CHECK(vkCreateDevice(pPhysDevice->Get(), &deviceCreateInfo, nullptr, &device));
            return device != VK_NULL_HANDLE;
        });
        
        VK_ASSERT(IsCreated());
        
        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(Get(), queueFamilyIndex, 0, &queue);
        m_queue.Create(this, queue, queueFamilyIndex);

        m_queue.SetDebugName("DEVICE_GFX_CMP_TRANSFER_QUEUE");

        return *this;
    }


    Device& Device::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        m_pPhysDevice = VK_NULL_HANDLE;
        m_queue.Destroy();

        Base::Destroy([](VkDevice& device) {
            vkDestroyDevice(device, nullptr);
        });

        return *this;
    }


    const Device& Device::WaitIdle() const
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkDeviceWaitIdle(Get()));

        return *this;
    }


    PFN_vkVoidFunction Device::GetProcAddr(std::string_view procName) const
    {
        VK_ASSERT(IsCreated());

        PFN_vkVoidFunction pFunc = vkGetDeviceProcAddr(Get(), procName.data());
        VK_ASSERT_MSG(pFunc != nullptr, "Failed to load Vulkan function: %s", procName.data());

        return pFunc;
    }


    PhysicalDevice& Device::GetPhysDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pPhysDevice;
    }


    const Queue& Device::GetQueue() const
    {
        VK_ASSERT(IsCreated());
        return m_queue;
    }


    Queue& Device::GetQueue()
    {
        VK_ASSERT(IsCreated());
        return m_queue;
    }
}