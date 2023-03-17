#include "mesh.h"

#include "../vulkan_layer/vulkan_layer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void Mesh::record_draw(vk::CommandBuffer& cmd_buffer, const vk::PipelineLayout& pipe_layout, const glm::mat4& view,
                       const glm::mat4& proj) {
  update_projection_buffer(view, proj);
  cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                pipe_layout, 0, 1, &desc_set.set, 0,
                                nullptr);

  std::vector<vk::Buffer> vertex_buffers = {vertecies};
  std::vector<vk::DeviceSize> offsets = {0};
  cmd_buffer.bindVertexBuffers(0, vertex_buffers, offsets);

  cmd_buffer.bindIndexBuffer(indices, {0}, vk::IndexType::eUint32);

  cmd_buffer.drawIndexed(num_indices, 1, 0, 0, 0);
}

void Mesh::update_projection_buffer(const glm::mat4& view,
                                    const glm::mat4& proj) {
  const auto to_view = view * entity_to_world;
  const auto normal_matrix = glm::transpose(glm::inverse(to_view));
  const auto render_matrix = proj * to_view;
  MeshProjectionData proj_data{
      .to_view = to_view,
      .normal_to_view = normal_matrix,
      .to_screen = render_matrix,
  };

  std::memcpy(proj_buffer_ptr, &proj_data, sizeof(MeshProjectionData));
};

Mesh Mesh::load(const std::string& filepath) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        filepath.c_str())) {
    throw std::runtime_error(warn + err);
  }

  std::vector<Vertex> vertecies;
  std::vector<uint32_t> indices;

  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      Vertex vertex{};

      vertex.position = {attrib.vertices[3 * index.vertex_index + 0],
                         attrib.vertices[3 * index.vertex_index + 1],
                         attrib.vertices[3 * index.vertex_index + 2]};

      vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                       attrib.normals[3 * index.normal_index + 1],
                       attrib.normals[3 * index.normal_index + 2]};

      vertex.tex_coord = {attrib.texcoords[2 * index.texcoord_index + 0],
                          1.f - attrib.texcoords[2 * index.texcoord_index + 1]};

      vertecies.push_back(vertex);
      indices.push_back(indices.size());
    }
  }

  Mesh output;

  output.num_indices = indices.size();

  size_t vertex_buffer_size = vertecies.size() * sizeof(Vertex);
  auto buffer = VulkanLayer::get_instance().create_buffer(
      vertex_buffer_size, vk::BufferUsageFlagBits::eVertexBuffer);

  void* data;
  buffer.map(data);

  memcpy(data, vertecies.data(), vertex_buffer_size);

  buffer.unmap();
  output.vertecies = buffer.buffer;

  size_t index_buffer_size = indices.size() * sizeof(uint32_t);
  buffer = VulkanLayer::get_instance().create_buffer(
      index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer);

  buffer.map(data);

  memcpy(data, indices.data(), index_buffer_size);

  buffer.unmap();
  output.indices = buffer.buffer;

  return output;
}
