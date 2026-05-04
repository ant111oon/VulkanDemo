#include "pch.h"

#include "vk_descriptor.h"
#include "vk_texture.h"


namespace vkn
{
    static PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSize = nullptr;
    static PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffset = nullptr;
    static PFN_vkGetDescriptorEXT vkGetDescriptor = nullptr;


    static VkDeviceSize GetAlignedSize(VkDeviceSize value, VkDeviceSize alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }


    static void* GetDescriptorBindingPtr(Buffer& buffer, VkDeviceSize setOffset, VkDeviceSize bindingOffset, uint32_t elemIdx, VkDeviceSize descrSize)
    {
        return (char*)buffer.Map() + setOffset + bindingOffset + (elemIdx * descrSize);
    }


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
        std::swap(m_descriptors, layout.m_descriptors);
        std::swap(m_size, layout.m_size);
        std::swap(m_state, layout.m_state);

        Base::operator=(std::move(layout));

        return *this;
    }
    

    DescriptorSetLayout& DescriptorSetLayout::Create(const DescriptorSetLayoutCreateInfo& info)
    {
        return Create(info.pDevice, info.flags, info.descriptorInfos);
    }


    DescriptorSetLayout& DescriptorSetLayout::Create(Device* pDevice, VkDescriptorSetLayoutCreateFlags flags, std::span<const DescriptorInfo> descriptorInfos)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of descriptor layout %s", GetDebugName().data());
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

        if (vkGetDescriptor == nullptr) {
            vkGetDescriptor = (PFN_vkGetDescriptorEXT)pDevice->GetProcAddr("vkGetDescriptorEXT");
        }

        std::vector<VkDescriptorSetLayoutBinding> bindings(descriptorInfos.size());
        
        std::vector<VkDescriptorBindingFlags> bindingsFlags(descriptorInfos.size());
        bindingsFlags.clear();

        m_descriptors.resize(m_descriptors.size());

        bool needSort = false;
        int64_t lastBinding = INT64_MIN;

        for (size_t i = 0; i < bindings.size(); ++i) {
            const DescriptorInfo& descriptorInfo = descriptorInfos[i];

            needSort = needSort || descriptorInfo.binding < lastBinding;
            lastBinding = descriptorInfo.binding;

            VkDescriptorSetLayoutBinding& binding = bindings[i];
            
            binding.binding = descriptorInfo.binding;
            binding.descriptorType = descriptorInfo.type;
            binding.descriptorCount = descriptorInfo.count;
            binding.stageFlags = descriptorInfo.stagesMask;

            if (descriptorInfo.flags != 0) {
                bindingsFlags.emplace_back(descriptorInfo.flags);
            }

            Descriptor descriptor = {};
            descriptor.binding = descriptorInfo.binding;
            descriptor.type = descriptorInfo.type;
            descriptor.count = descriptorInfo.count;

            m_descriptors.emplace_back(descriptor);
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

        Base::Create([pDevice, &createInfo](VkDescriptorSetLayout& layout) {
            VK_CHECK(vkCreateDescriptorSetLayout(pDevice->Get(), &createInfo, nullptr, &layout));
            return layout != VK_NULL_HANDLE;
        });
        
        VK_ASSERT(IsCreated());

        m_pDevice = pDevice;

        if ((flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT) != 0) {
            m_state.set(BIT_IS_DESCRIPTOR_BUFFER_SOMPATIBLE, true);

            vkGetDescriptorSetLayoutSize(pDevice->Get(), Get(), &m_size);
            m_size = GetAlignedSize(m_size, pDevice->GetPhysDevice().GetDescBufferProperties().descriptorBufferOffsetAlignment);
    
            for (size_t i = 0; i < bindings.size(); ++i) {
                vkGetDescriptorSetLayoutBindingOffset(pDevice->Get(), Get(), descriptorInfos[i].binding, &m_descriptors[i].offset);
            }
        }

        if (needSort) {
            std::sort(m_descriptors.begin(), m_descriptors.end(), [](const Descriptor& l, const Descriptor& r) {
                return l.binding < r.binding;
            });
        }

        return *this;
    }


    DescriptorSetLayout& DescriptorSetLayout::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }


        Base::Destroy([vkDevice = m_pDevice->Get()](VkDescriptorSetLayout& layout) {
            vkDestroyDescriptorSetLayout(vkDevice, layout, nullptr);        
        });
        
        m_pDevice = nullptr;
        
        m_size = 0;
        m_descriptors = {};

        m_state = {};

        return *this;
    }


    DescriptorSetLayout::Descriptor& DescriptorSetLayout::GetDescriptorByIdx(uint32_t index)
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(index < m_descriptors.size());

        return m_descriptors[index];
    }


    const DescriptorSetLayout::Descriptor& DescriptorSetLayout::GetDescriptorByIdx(uint32_t index) const
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(index < m_descriptors.size());

        return m_descriptors[index];
    }


    DescriptorSetLayout::Descriptor& DescriptorSetLayout::GetDescriptorByBinding(uint32_t binding)
    {
        VK_ASSERT(IsCreated());
        
        const uint64_t index = GetDescriptorIndex(binding);
        VK_ASSERT_MSG(index != UINT64_MAX, "Failed to find descriptor with binding %u in %s", binding, GetDebugName().data());

        return m_descriptors[index];
    }


    const DescriptorSetLayout::Descriptor& DescriptorSetLayout::GetDescriptorByBinding(uint32_t binding) const
    {
        VK_ASSERT(IsCreated());

        const uint64_t index = GetDescriptorIndex(binding);
        VK_ASSERT(index != UINT64_MAX);

        return m_descriptors[index];
    }


    bool DescriptorSetLayout::HasDescriptor(uint32_t binding) const
    {
        VK_ASSERT(IsCreated());
        return GetDescriptorIndex(binding) != UINT64_MAX;
    }


    size_t DescriptorSetLayout::GetDescriptorsCount() const
    {
        VK_ASSERT(IsCreated());
        return m_descriptors.size();
    }


    Device& DescriptorSetLayout::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }


    // Size of descriptor set in bytes
    VkDeviceSize DescriptorSetLayout::GetSize() const
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(IsDescriptorBufferCompatible());

        return m_size;
    }


    bool DescriptorSetLayout::IsDescriptorBufferCompatible() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_DESCRIPTOR_BUFFER_SOMPATIBLE);
    }


    uint64_t DescriptorSetLayout::GetDescriptorIndex(uint32_t binding) const
    {
        auto it = std::lower_bound(m_descriptors.cbegin(), m_descriptors.cend(), binding, [](const Descriptor& a, uint32_t value){
            return a.binding < value;
        });

        if (it != m_descriptors.cend() && it->binding == binding) {
            return static_cast<uint64_t>(it - m_descriptors.cbegin());
        }

        return UINT64_MAX;
    }


    DescriptorBuffer::DescriptorBuffer(Device* pDevice, std::span<DescriptorSetLayout*> layouts)
    {
        Create(pDevice, layouts);
    }


    DescriptorBuffer::DescriptorBuffer(const DescriptorBufferCreateInfo& info)
    {
        Create(info);
    }


    DescriptorBuffer::~DescriptorBuffer()
    {
        Destroy();
    }


    DescriptorBuffer::DescriptorBuffer(DescriptorBuffer&& buffer) noexcept
    {
        *this = std::move(buffer);
    }


    DescriptorBuffer& DescriptorBuffer::operator=(DescriptorBuffer&& buffer) noexcept
    {
        if (this == &buffer) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_buffer, buffer.m_buffer);
        std::swap(m_entries, buffer.m_entries);

        return *this;
    }


    DescriptorBuffer& DescriptorBuffer::Create(Device* pDevice, std::span<DescriptorSetLayout*> layouts)
    {
        DescriptorBufferCreateInfo info = {};
        info.pDevice = pDevice;
        info.layouts = layouts;

        return Create(info);
    }


    DescriptorBuffer& DescriptorBuffer::Create(const DescriptorBufferCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of buffer %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT(!info.layouts.empty());

        VkDeviceSize bufferSize = 0;

        m_entries.resize(info.layouts.size());
        for (size_t i = 0; i < info.layouts.size(); ++i) {
            Entry& entry = m_entries[i];

            entry.pLayout = info.layouts[i];
            VK_ASSERT(entry.pLayout != nullptr);

            entry.offset = bufferSize;

            bufferSize += entry.pLayout->GetSize();
        }

        const VkBufferUsageFlags2 usage = VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;

        AllocationInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_buffer.Create(info.pDevice, bufferSize, usage, allocInfo);

        VK_ASSERT(m_buffer.IsCreated());

        return *this;
    }


    DescriptorBuffer& DescriptorBuffer::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        m_buffer.Destroy();
        m_entries = {};

        return *this;
    }


    DescriptorBuffer& DescriptorBuffer::WriteDescriptor(uint32_t setIdx, uint32_t binding, uint32_t elemIdx, const Buffer& buffer)
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT_MSG(buffer.HasDeviceAddress(), "Buffer %s must have device address to be compatible with descriptor buffer", buffer.GetDebugName().data());

        Device& device = GetDevice();

        const Entry& entry = m_entries[setIdx];

        const VkDeviceSize setOffset = entry.offset;
        const VkDeviceSize bindingOffset = entry.pLayout->GetDescriptorByBinding(binding).offset;

        const VkPhysicalDeviceDescriptorBufferPropertiesEXT& buffProps = device.GetPhysDevice().GetDescBufferProperties();
        const size_t descrSize = buffer.IsUniformBuffer() ? buffProps.uniformBufferDescriptorSize : buffProps.storageBufferDescriptorSize;

        void* pBindingPtr = GetDescriptorBindingPtr(m_buffer, setOffset, bindingOffset, elemIdx, descrSize);

        VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
        descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;
        descriptorAddressInfo.address = buffer.GetDeviceAddress();
        descriptorAddressInfo.range = buffer.GetMemorySize();

        VkDescriptorGetInfoEXT descriptorGetInfo = {};
        descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorGetInfo.type = buffer.IsUniformBuffer() ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;

        vkGetDescriptor(device.Get(), &descriptorGetInfo, descrSize, pBindingPtr);

        return *this;
    }


    DescriptorBuffer& DescriptorBuffer::WriteDescriptor(uint32_t setIdx, uint32_t binding, uint32_t elemIdx, const TextureView& view)
    {
        VK_ASSERT(IsCreated());
        view.CheckLayoutConsistency();

        Device& device = GetDevice();

        const Entry& entry = m_entries[setIdx];

        const VkDeviceSize setOffset = entry.offset;
        const VkDeviceSize bindingOffset = entry.pLayout->GetDescriptorByBinding(binding).offset;

        const size_t descrSize = device.GetPhysDevice().GetDescBufferProperties().sampledImageDescriptorSize;

        void* pBindingPtr = GetDescriptorBindingPtr(m_buffer, setOffset, bindingOffset, elemIdx, descrSize);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = view.Get();
        
        const vkn::Texture& owner = view.GetOwner();
        const TextureView::SubresourceRange& range = view.GetSubresourceRange();
        imageInfo.imageLayout = owner.GetAccessState(range.baseArrayLayer, range.baseMipLevel).layout;

        VkDescriptorGetInfoEXT descriptorGetInfo = {};
        descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorGetInfo.data.pSampledImage = &imageInfo;

        vkGetDescriptor(device.Get(), &descriptorGetInfo, descrSize, pBindingPtr);

        return *this;
    }


    DescriptorBuffer& DescriptorBuffer::WriteDescriptor(uint32_t setIdx, uint32_t binding, uint32_t elemIdx, const Sampler& sampler)
    {
        VK_ASSERT(IsCreated());

        Device& device = GetDevice();

        const Entry& entry = m_entries[setIdx];

        const VkDeviceSize setOffset = entry.offset;
        const VkDeviceSize bindingOffset = entry.pLayout->GetDescriptorByBinding(binding).offset;

        const size_t descrSize = device.GetPhysDevice().GetDescBufferProperties().samplerDescriptorSize;

        void* pBindingPtr = GetDescriptorBindingPtr(m_buffer, setOffset, bindingOffset, elemIdx, descrSize);

        VkDescriptorGetInfoEXT descriptorGetInfo = {};
        descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorGetInfo.data.pSampler = &sampler.Get();

        vkGetDescriptor(device.Get(), &descriptorGetInfo, descrSize, pBindingPtr);

        return *this;
    }


    VkDeviceSize DescriptorBuffer::GetSetOffset(uint32_t index) const
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(index < m_entries.size());

        return m_entries[index].offset;
    }


    const DescriptorSetLayout* DescriptorBuffer::GetDescriptorSetLayout(uint32_t index) const
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(index < m_entries.size());

        return m_entries[index].pLayout;
    }


    std::string_view DescriptorBuffer::GetDebugName() const
    {
        return m_buffer.GetDebugName();
    }


    size_t DescriptorBuffer::GetSetCount() const
    {
        VK_ASSERT(IsCreated());
        return m_entries.size();
    }


    Device& DescriptorBuffer::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return m_buffer.GetDevice();
    }


    const Buffer& DescriptorBuffer::Get() const
    {
        VK_ASSERT(IsCreated());
        return m_buffer;
    }


    bool DescriptorBuffer::IsCreated() const
    {
        return m_buffer.IsCreated();
    }
}