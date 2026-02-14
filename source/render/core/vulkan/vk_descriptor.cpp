#include "vk_descriptor.h"


namespace vkn
{
    static PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSize = nullptr;
    static PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffset = nullptr;


    static VkDeviceSize GetAlignedSize(VkDeviceSize value, VkDeviceSize alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    };


    DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
    {
        Create(info);
    }


    DescriptorSetLayout::DescriptorSetLayout(Device* pDevice, VkDescriptorSetLayoutCreateFlags flags, std::span<const DescriptorInfo> descriptorInfos)
    {
        Create(pDevice, flags, descriptorInfos);
    }
    

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        Destroy();
    }


    DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& layout) noexcept
    {
        *this = std::move(layout);
    }


    DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& layout) noexcept
    {
        if (this == &layout) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, layout.m_pDevice);
        std::swap(m_layout, layout.m_layout);
        std::swap(m_descriptorOffsets, layout.m_descriptorOffsets);
        std::swap(m_size, layout.m_size);
        std::swap(m_state, layout.m_state);

        Object::operator=(std::move(layout));

        return *this;
    }
    

    DescriptorSetLayout& DescriptorSetLayout::Create(const DescriptorSetLayoutCreateInfo& info)
    {
        return Create(info.pDevice, info.flags, info.descriptorInfos);
    }


    DescriptorSetLayout& DescriptorSetLayout::Create(Device* pDevice, VkDescriptorSetLayoutCreateFlags flags, std::span<const DescriptorInfo> descriptorInfos)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of descriptor layout %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(pDevice && pDevice->IsCreated());
        VK_ASSERT(!descriptorInfos.empty());

        if (vkGetDescriptorSetLayoutSize == nullptr) {
            vkGetDescriptorSetLayoutSize = (PFN_vkGetDescriptorSetLayoutSizeEXT)pDevice->GetProcAddr("vkGetDescriptorSetLayoutSizeEXT");
        }

        if (vkGetDescriptorSetLayoutBindingOffset == nullptr) {
            vkGetDescriptorSetLayoutBindingOffset = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)pDevice->GetProcAddr("vkGetDescriptorSetLayoutBindingOffsetEXT");
        }

        std::vector<VkDescriptorSetLayoutBinding> bindings(descriptorInfos.size());
        
        std::vector<VkDescriptorBindingFlags> bindingsFlags(descriptorInfos.size());
        bindingsFlags.clear();

        for (size_t i = 0; i < bindings.size(); ++i) {
            const DescriptorInfo& descriptorInfo = descriptorInfos[i];
            
            VkDescriptorSetLayoutBinding& binding = bindings[i];
            
            binding.binding = descriptorInfo.binding;
            binding.descriptorType = descriptorInfo.type;
            binding.descriptorCount = descriptorInfo.count;
            binding.stageFlags = descriptorInfo.stagesMask;

            if (descriptorInfo.flags != 0) {
                bindingsFlags.emplace_back(descriptorInfo.flags);
            }
        }

        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.flags = flags;
        createInfo.bindingCount = bindings.size();
        createInfo.pBindings = bindings.data();

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
        bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
        bindingFlagsCreateInfo.bindingCount = bindingsFlags.size();
        bindingFlagsCreateInfo.pBindingFlags = bindingsFlags.data();

        createInfo.pNext = &bindingFlagsCreateInfo;

        VK_CHECK(vkCreateDescriptorSetLayout(pDevice->Get(), &createInfo, nullptr, &m_layout));
        VK_ASSERT(m_layout != VK_NULL_HANDLE);

        m_pDevice = pDevice;

        if ((flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT) != 0) {
            m_state.set(BIT_IS_DESCRIPTOR_BUFFER_SOMPATIBLE, true);

            vkGetDescriptorSetLayoutSize(pDevice->Get(), m_layout, &m_size);
            m_size = GetAlignedSize(m_size, pDevice->GetPhysDevice()->GetDescBufferProperties().descriptorBufferOffsetAlignment);

            m_descriptorOffsets.resize(descriptorInfos.size());
    
            for (size_t i = 0; i < bindings.size(); ++i) {
                vkGetDescriptorSetLayoutBindingOffset(pDevice->Get(), m_layout, descriptorInfos[i].binding, &m_descriptorOffsets[i]);
            }
        }

        SetCreated(true);

        return *this;
    }


    DescriptorSetLayout& DescriptorSetLayout::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vkDestroyDescriptorSetLayout(m_pDevice->Get(), m_layout, nullptr);        
        m_layout = VK_NULL_HANDLE;
        
        m_pDevice = nullptr;
        
        m_size = 0;
        m_descriptorOffsets = {};

        m_state = {};

        Object::Destroy();

        return *this;
    }
}