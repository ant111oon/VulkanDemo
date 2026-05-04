#include "pch.h"

#include "vk_resource_access_tracker.h"


namespace vkn
{
    bool BufferAccessTracker::State::operator==(const State &state) const
    {
        return stageMask == state.stageMask && accessMask == state.accessMask;
    }


    BufferAccessTracker::~BufferAccessTracker()
    {
        Destroy();
    }

    
    void BufferAccessTracker::Create()
    {
        m_accessState.stageMask = VK_PIPELINE_STAGE_2_NONE;
        m_accessState.accessMask = VK_ACCESS_2_NONE;
    }

    
    void BufferAccessTracker::Destroy()
    {
        m_accessState.stageMask = VK_PIPELINE_STAGE_2_NONE;
        m_accessState.accessMask = VK_ACCESS_2_NONE;
    }


    void BufferAccessTracker::Transit(VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
    {
        m_accessState.stageMask = dstStageMask;
        m_accessState.accessMask = dstAccessMask;
    }

    
    const BufferAccessTracker::State& BufferAccessTracker::GetState() const
    {
        return m_accessState;
    }


    bool TextureAccessTracker::State::operator==(const State &state) const
    {
        return layout == state.layout && stageMask == state.stageMask && accessMask == state.accessMask;
    }


    TextureAccessTracker::~TextureAccessTracker()
    {
        Destroy();
    }


    void TextureAccessTracker::Create(VkImageLayout initialLayout, uint32_t layerCount, uint32_t mipCount)
    {
        if (layerCount == 1 && mipCount == 1) {
            m_accessStates = State { initialLayout, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE };
        } else {
            m_accessStates = StateArray(layerCount * mipCount, 
                State { 
                    .layout = initialLayout, 
                    .stageMask = VK_PIPELINE_STAGE_2_NONE, 
                    .accessMask = VK_ACCESS_2_NONE
                }
            );
        }

        m_layerCount = layerCount;
        m_mipCount = mipCount;
    }


    void TextureAccessTracker::Destroy()
    {
        m_accessStates = State{};
    }


    void TextureAccessTracker::Transit(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount, VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
    {
        VK_ASSERT(mipCount >= 1);
        VK_ASSERT(layerCount >= 1);
        VK_ASSERT(baseMip + mipCount <= m_mipCount);
        VK_ASSERT(baseLayer + layerCount <= m_layerCount);

        auto FillState = [](State& outState, VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
        {
            outState.layout = dstLayout;
            outState.stageMask = dstStageMask;
            outState.accessMask = dstAccessMask;
        };

        if (std::holds_alternative<State>(m_accessStates)) {
            State& state = std::get<State>(m_accessStates);
            FillState(state, dstLayout, dstStageMask, dstAccessMask);
        } else {
            StateArray& states = std::get<StateArray>(m_accessStates);

            for (uint32_t i = 0; i < layerCount; ++i) {
                const uint32_t layer = baseLayer + i;

                for (uint32_t j = 0; j < mipCount; ++j) {
                    const uint32_t mip = baseMip + j;
                    const uint32_t index = layer * m_mipCount + mip;

                    FillState(states[index], dstLayout, dstStageMask, dstAccessMask);
                }
            }
        }
    }

    
    bool TextureAccessTracker::CheckLayoutConsistency(uint32_t baseLayer, uint32_t layerCount, uint32_t baseMip, uint32_t mipCount) const
    {
    #ifdef ENG_BUILD_DEBUG
        const uint32_t lastLayerIdx = baseLayer + layerCount - 1;
        const uint32_t lastMipIdx = baseMip + mipCount - 1;

        const VkImageLayout layout = GetState(baseLayer, baseMip).layout;

        for (uint32_t layerIdx = baseLayer; layerIdx <= lastLayerIdx; ++layerIdx) {
            for (uint32_t mipIdx = baseMip; mipIdx <= lastMipIdx; ++mipIdx) {
                if (layout != GetState(layerIdx, mipIdx).layout) {                    
                    return false;
                }
            }
        }
    #endif

        return true;
    }


    const TextureAccessTracker::State& TextureAccessTracker::GetState(uint32_t layer, uint32_t mip) const
    {
        VK_ASSERT(mip < m_mipCount);
        VK_ASSERT(layer < m_layerCount);

        if (std::holds_alternative<State>(m_accessStates)) {
            return std::get<State>(m_accessStates);
        } else {
            return std::get<StateArray>(m_accessStates)[layer * m_mipCount + mip];
        }
    }
}