#include "pch.h"

#include "vk_query.h"


namespace vkn
{
    QueryPool::QueryPool(const QueryCreateInfo& info)
        : Object()
    {
        Create(info);
    }


    QueryPool::QueryPool(QueryPool&& pool) noexcept
    {
        *this = std::move(pool);
    }


    QueryPool& QueryPool::operator=(QueryPool&& pool) noexcept
    {
        if (this == &pool) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        Object::operator=(std::move(pool));

        std::swap(m_pDevice, pool.m_pDevice);

        std::swap(m_pool, pool.m_pool);
        std::swap(m_queryCount, pool.m_queryCount);

        return *this; 
    }


    bool QueryPool::Create(const QueryCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("QueryPool %s is already created", GetDebugName());
            return false;
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkQueryPoolCreateInfo queryPoolCreateInfo = {};
        queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolCreateInfo.flags = info.flags;
        queryPoolCreateInfo.queryType = info.queryType;
        queryPoolCreateInfo.queryCount = info.queryCount;
        queryPoolCreateInfo.pipelineStatistics = info.pipelineStatistics;

        m_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateQueryPool(vkDevice, &queryPoolCreateInfo, nullptr, &m_pool));

        const bool isCreated = m_pool != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_pDevice = info.pDevice;
        m_queryCount = info.queryCount;

        SetCreated(isCreated);

        return isCreated;
    }


    void QueryPool::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        VkDevice vkDevice = m_pDevice->Get();

        vkDestroyQueryPool(vkDevice, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
        
        m_queryCount = 0;

        m_pDevice = nullptr;

        Object::Destroy();
    }


    void QueryPool::GetResults(uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) const
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(pData);
        VK_ASSERT(firstQuery + queryCount <= m_queryCount);

        const VkResult result = vkGetQueryPoolResults(m_pDevice->Get(), m_pool, firstQuery, queryCount, dataSize, pData, stride, flags);

        if (result == VK_NOT_READY) {
            return;
        }

        VK_CHECK(result);
    }


    bool QueryPool::IsQueryIndexValid(uint32_t queryIndex) const
    {
        return IsCreated() ? queryIndex < m_queryCount : false;
    }


    const char* QueryPool::GetDebugName() const
    {
        return Object::GetDebugName("QueryPool");
    }
}