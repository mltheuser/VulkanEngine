#pragma once

#include "../vulkan_layer/vulkan_layer.h"
#include "Texture.h"

class Material {
 public:
  Texture diffuse;

  Material(Texture diffuse) : diffuse{diffuse} { create_descriptor_set(); }

  void record_draw(vk::CommandBuffer& cmd_buffer,
                   const vk::PipelineLayout& pipe_layout) {
    cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipe_layout,
                                  1, 1, &desc_set.set, 0, nullptr);
  }

  static vk::DescriptorSetLayout get_descriptor_set_layout() {
    return VulkanLayer::get_instance().create_descriptor_set_layout(
        get_descriptor_set_info());
  }

 private:
  DescriptorSet desc_set;

  static DescriptorSetInfo get_descriptor_set_info() {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(1);
    bindings[0].binding = 0;
    bindings[0].setStageFlags(vk::ShaderStageFlagBits::eFragment);
    bindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[0].descriptorCount = 1;

    return DescriptorSetInfo(vk::DescriptorSetLayoutCreateInfo(), bindings);
  }

  void create_descriptor_set() {
    auto set_layout_info = get_descriptor_set_info();

    desc_set =
        VulkanLayer::get_instance().allocate_descriptor_set(set_layout_info);

    vk::DescriptorImageInfo diff_info;
    diff_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    diff_info.imageView = diffuse.view.view;
    diff_info.sampler = diffuse.sampler;

    std::vector<vk::WriteDescriptorSet> writes(1);
    writes[0].dstSet = desc_set.set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &diff_info;

    VulkanLayer::get_instance().device.updateDescriptorSets(writes, {});
  }
};