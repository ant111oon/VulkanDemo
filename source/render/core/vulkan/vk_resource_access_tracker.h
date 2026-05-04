#pragma once

#include "vk_core.h"

#include <variant>


namespace vkn
{
    class BufferAccessTracker
    {
    public:
        struct State
        {
            bool operator==(const State& state) const;

            VkPipelineStageFlags2 stageMask = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        accessMask = VK_ACCESS_2_NONE;
        };

    public:
        BufferAccessTracker() = default;
        ~BufferAccessTracker();

        void Create();
        void Destroy();

        void Transit(VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

        const State& GetState() const;

    private:
        State m_accessState = {};
    };


    class TextureAccessTracker
    {
    public:
        struct State
        {
            bool operator==(const State& state) const;

            VkImageLayout         layout = VK_IMAGE_LAYOUT_UNDEFINED; 
            VkPipelineStageFlags2 stageMask = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        accessMask = VK_ACCESS_2_NONE;
        };

    public:
        TextureAccessTracker() = default;
        ~TextureAccessTracker();

        void Create(VkImageLayout initialLayout, uint32_t layerCount, uint32_t mipCount);
        void Destroy();

        void Transit(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount,
            VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

        bool CheckLayoutConsistency(uint32_t baseLayer, uint32_t layerCount, uint32_t baseMip, uint32_t mipCount) const;

        const State& GetState(uint32_t layer, uint32_t mip) const;

    private:
        using StateArray = std::vector<State>;

        std::variant<State, StateArray> m_accessStates = State{};

        uint32_t m_layerCount = 0;
        uint32_t m_mipCount = 0;
    };
}