#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    struct QueryCreateInfo
    {
        Device* pDevice;
        VkQueryPipelineStatisticFlags pipelineStatistics;
        VkQueryPoolCreateFlags flags;
        VkQueryType queryType;
        uint32_t queryCount;
    };


    class QueryPool : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(QueryPool);

        QueryPool() = default;
        QueryPool(const QueryCreateInfo& info);

        QueryPool(QueryPool&& pool) noexcept;
        QueryPool& operator=(QueryPool&& pool) noexcept;

        bool Create(const QueryCreateInfo& info);
        void Destroy();

        void GetResults(uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *pData, VkDeviceSize stride, VkQueryResultFlags flags) const;

        bool IsQueryIndexValid(uint32_t queryIndex) const;

        void SetDebugName(const char* pName) { Object::SetDebugName(m_pDevice->Get(), (uint64_t)m_pool, VK_OBJECT_TYPE_QUERY_POOL, pName); }
        const char* GetDebugName() const { return Object::GetDebugName("QueryPool"); }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkQueryPool Get() const
        {
            VK_ASSERT(IsCreated());
            return m_pool;
        }

        size_t GetQueryCount() const
        {
            VK_ASSERT(IsCreated());
            return m_queryCount;
        }

    private:
        Device* m_pDevice = nullptr;

        VkQueryPool m_pool = VK_NULL_HANDLE;
        size_t m_queryCount = 0;
    };
}