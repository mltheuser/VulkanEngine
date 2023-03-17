#pragma once
#include <vector>

#include "../vulkan_layer/vulkan_layer.h"
#include "entity.h"

struct MeshProjectionData {
  glm::mat4 to_view;
  glm::mat4 normal_to_view;
  glm::mat4 to_screen;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 tex_coord;
};

class Mesh : public Entity {
 public:
  vk::Buffer vertecies;
  vk::Buffer indices;

  uint32_t num_indices;

  Mesh() {
    create_projection_buffer();
    create_descriptor_set();
  }

  void record_draw(vk::CommandBuffer& cmd_buffer,
                   const vk::PipelineLayout& pipe_layout, const glm::mat4& view,
                   const glm::mat4& proj);

  void update_projection_buffer(const glm::mat4& view, const glm::mat4& proj);

  static Mesh load(const std::string& filepath);

  static vk::DescriptorSetLayout get_descriptor_set_layout() {
    return VulkanLayer::get_instance().create_descriptor_set_layout(
        get_descriptor_set_info());
  }

 private:
  Buffer proj_buffer;
  void* proj_buffer_ptr;

  DescriptorSet desc_set;

  static DescriptorSetInfo get_descriptor_set_info() {
    vk::DescriptorSetLayoutBinding model_mat_binding;
    model_mat_binding.binding = 0;
    model_mat_binding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    model_mat_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    model_mat_binding.descriptorCount = 1;

    std::vector<vk::DescriptorSetLayoutBinding> bindings{model_mat_binding};

    return DescriptorSetInfo(vk::DescriptorSetLayoutCreateInfo(), bindings);
  }

  void create_projection_buffer() {
    proj_buffer = VulkanLayer::get_instance().create_buffer(
        sizeof(MeshProjectionData), vk::BufferUsageFlagBits::eUniformBuffer);
    proj_buffer.map(proj_buffer_ptr);
  }

  void create_descriptor_set() {
    auto set_layout_info = get_descriptor_set_info();

    desc_set =
        VulkanLayer::get_instance().allocate_descriptor_set(set_layout_info);

    vk::DescriptorBufferInfo proj_buffer_info;
    proj_buffer_info.buffer = proj_buffer.buffer;
    proj_buffer_info.range = VK_WHOLE_SIZE;

    std::vector<vk::WriteDescriptorSet> writes(1);
    writes[0].dstSet = desc_set.set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &proj_buffer_info;
    VulkanLayer::get_instance().device.updateDescriptorSets(writes, {});
  }
};