#pragma once

#include "vk_object.h"
#include "vk_device.h"

#include <array>


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

        template <typename T>
        void GetResults(uint32_t firstQuery, uint32_t queryCount, std::span<T> outData, VkQueryResultFlags flags) const
        {
            VK_ASSERT(queryCount <= outData.size());
            GetResults(firstQuery, queryCount, outData.size() * sizeof(T), outData.data(), sizeof(T), flags);
        }

        template <typename T, size_t QUERY_COUNT>
        std::array<T, QUERY_COUNT> GetResults(uint32_t firstQuery, VkQueryResultFlags flags) const
        {
            std::array<T, QUERY_COUNT> outData = {};
            GetResults(firstQuery, QUERY_COUNT, QUERY_COUNT * sizeof(T), outData.data(), sizeof(T), flags);

            return outData;
        }

        bool IsQueryIndexValid(uint32_t queryIndex) const;

        void SetDebugName(const char* pName);
        const char* GetDebugName() const;

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