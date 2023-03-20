#pragma once

#include <vulkan/vulkan.hpp>

#include "../../third_party/vkbootstrap/VkBootstrap.h"
#include "vk_mem_alloc.h"

class Buffer {
 public:
  vk::Buffer buffer;

  Buffer(){};
  Buffer(vk::Buffer buffer, VmaAllocator& allocator, VmaAllocation& alloc_info)
      : buffer{buffer}, allocator{allocator}, alloc_info{alloc_info} {};

  void map(void*& ptr) { vmaMapMemory(allocator, alloc_info, &ptr); }
  void unmap() { vmaUnmapMemory(allocator, alloc_info); }

 private:
  VmaAllocator allocator;
  VmaAllocation alloc_info;
};

class Image {
 public:
  vk::Image image;
  Image(){};
  Image(vk::Image image) : image{image} {};
  Image(vk::Image image, VmaAllocator& allocator, VmaAllocation& alloc_info)
      : image{image}, allocator{allocator}, alloc_info{alloc_info} {};

  void map(void*& ptr) { vmaMapMemory(allocator, alloc_info, &ptr); }
  void unmap() { vmaUnmapMemory(allocator, alloc_info); }

 private:
  VmaAllocator allocator;
  VmaAllocation alloc_info;
};

class ImageView {
 public:
  Image image;
  vk::ImageView view;
  ImageView(){};
  ImageView(Image image, vk::ImageView view) : image{image}, view{view} {};
};

class DescriptorSetInfo {
 public:
  vk::DescriptorSetLayoutCreateInfo info;
  std::vector<vk::DescriptorSetLayoutBinding> bindings;

  DescriptorSetInfo() {}
  DescriptorSetInfo(vk::DescriptorSetLayoutCreateInfo info,
                    std::vector<vk::DescriptorSetLayoutBinding> bindings)
      : info{info}, bindings{bindings} {
    this->info.setBindings(this->bindings);
  }
};

struct DescriptorSet {
  DescriptorSetInfo info;
  vk::DescriptorSetLayout layout;
  vk::DescriptorSet set;
};

void VK_CHECK(vk::Result x);

class VulkanLayer {
 public:
  static VulkanLayer& get_instance() {
    static VulkanLayer instance;
    return instance;
  }

  vk::Instance instance;
  vk::PhysicalDevice physical_device;
  vk::Device device;

  uint32_t graphics_queue_family;
  vk::Queue graphics_queue;
  vk::CommandPool graphics_command_pool;

  void record_layout_transition(vk::CommandBuffer& cmd_buffer, vk::Image image,
                                vk::ImageLayout old_layout,
                                vk::ImageLayout new_layout,
                                vk::PipelineStageFlags src_stage,
                                vk::PipelineStageFlags dst_stage,
                                vk::AccessFlags src_access_mask,
                                vk::AccessFlags dst_access_mask,
                                vk::ImageAspectFlags aspect_mask) {
    vk::ImageMemoryBarrier img_mem_barrier;
    img_mem_barrier.srcAccessMask = src_access_mask;
    img_mem_barrier.dstAccessMask = dst_access_mask;
    img_mem_barrier.oldLayout = old_layout;
    img_mem_barrier.newLayout = new_layout;
    img_mem_barrier.setImage(image);

    vk::ImageSubresourceRange sub_range;
    sub_range.aspectMask = aspect_mask;
    sub_range.baseArrayLayer = 0;
    sub_range.baseMipLevel = 0;
    sub_range.layerCount = 1;
    sub_range.levelCount = 1;
    img_mem_barrier.subresourceRange = sub_range;

    std::vector<vk::ImageMemoryBarrier> img_mem_barriers{img_mem_barrier};
    cmd_buffer.pipelineBarrier(src_stage, dst_stage,
                               vk::DependencyFlagBits::eByRegion, {}, {},
                               img_mem_barriers);
  }

  Buffer create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage) {
    vk::BufferCreateInfo bci;
    bci.usage = usage;
    bci.sharingMode = vk::SharingMode::eExclusive;
    bci.size = size;

    VmaAllocationCreateInfo aci = {};
    aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkBuffer tmp_buffer;
    VmaAllocation alloc_info;
    VkBufferCreateInfo bci_c = static_cast<VkBufferCreateInfo>(bci);
    vmaCreateBuffer(allocator, &bci_c, &aci, &tmp_buffer, &alloc_info, nullptr);

    auto buffer = vk::Buffer(tmp_buffer);

    return Buffer(buffer, allocator, alloc_info);
  }

  vk::ImageCreateInfo image2d_create_info(vk::Format format,
                                          vk::ImageUsageFlags usageFlags,
                                          vk::Extent3D extent);

  ImageView create_2d_image_view(vk::Extent2D extend, vk::Format format,
                                 vk::ImageUsageFlags usage,
                                 vk::ImageAspectFlags aspect,
                                 VmaMemoryUsage mem_usage);

  ImageView create_image_view(Image& image, vk::Format format,
                              vk::ImageAspectFlags aspect) {
    auto ivi = image_view2d_create_info(image.image, format, aspect);
    return ImageView(image, device.createImageView(ivi));
  };

  vk::ImageViewCreateInfo image_view2d_create_info(vk::Image& image,
                                                   vk::Format format,
                                                   vk::ImageAspectFlags aspect);

  vk::CommandBuffer create_command_buffer(vk::CommandPool& cmd_pool);

  vk::Sampler create_sampler() {
    vk::SamplerCreateInfo ci;
    ci.anisotropyEnable = false;
    ci.mipmapMode = vk::SamplerMipmapMode::eLinear;
    ci.unnormalizedCoordinates = false;

    ci.addressModeU = vk::SamplerAddressMode::eRepeat;
    ci.addressModeV = vk::SamplerAddressMode::eRepeat;
    ci.addressModeW = vk::SamplerAddressMode::eRepeat;

    ci.compareEnable = false;
    ci.magFilter = vk::Filter::eNearest;
    ci.minFilter = vk::Filter::eNearest;

    return device.createSampler(ci);
  }

  std::vector<char> readFile(const std::string& filename);

  vk::ShaderModule read_shader(const std::string& filename);

  vk::PipelineShaderStageCreateInfo create_shader_stage(
      const std::string& filename, const vk::ShaderStageFlagBits stage);

  vk::Fence create_fence() {
    vk::FenceCreateInfo fci;
    fci.flags = vk::FenceCreateFlagBits::eSignaled;
    return device.createFence(fci);
  }

  vk::Semaphore create_semaphore() {
    vk::SemaphoreCreateInfo sci;
    return device.createSemaphore(sci);
  }

  vk::DescriptorSetLayout create_descriptor_set_layout(
      const DescriptorSetInfo& layout_info) {
    return device.createDescriptorSetLayout(layout_info.info);
  }

  DescriptorSet allocate_descriptor_set(const DescriptorSetInfo& layout_info) {
    // Determine required descriptor pool sizes
    std::vector<vk::DescriptorPoolSize> poolSizes;
    for (const auto& layoutBinding : layout_info.bindings) {
      bool found = false;
      for (auto& poolSize : poolSizes) {
        if (poolSize.type == layoutBinding.descriptorType) {
          poolSize.descriptorCount += layoutBinding.descriptorCount;
          found = true;
          break;
        }
      }
      if (!found) {
        vk::DescriptorPoolSize poolSize;
        poolSize.type = layoutBinding.descriptorType;
        poolSize.descriptorCount = layoutBinding.descriptorCount;
        poolSizes.push_back(poolSize);
      }
    }

    // Create descriptor pool
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    auto descriptor_pool = device.createDescriptorPool(poolInfo);

    auto descriptor_set_layout = create_descriptor_set_layout(layout_info);

    // Allocate descriptor set from descriptor pool
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptor_set_layout;

    auto descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

    return DescriptorSet{.info = layout_info,
                         .layout = descriptor_set_layout,
                         .set = descriptorSet};
  }

 private:
  VmaAllocator allocator;
  vkb::Instance vkb_instance;

  VulkanLayer() { init_vulkan(); }

  bool init_vulkan();

  void setup_device();
  void setup_queues();
  void setup_cmd_pools();
  bool init_memory_allocator();
  vk::CommandPool create_command_pool(
      uint32_t family, vk::CommandPoolCreateFlags flags =
                           vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
  ~VulkanLayer();
};