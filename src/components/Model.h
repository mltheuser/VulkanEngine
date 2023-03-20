#include "Material.h"
#include "mesh.h"

class Model {
 public:
  Mesh mesh;
  Material material;

  Model(Mesh& mesh, Material& material) : mesh{mesh}, material{material} {}

  void record_draw(vk::CommandBuffer& cmd_buffer,
                   const vk::PipelineLayout& pipe_layout, const glm::mat4& view,
                   const glm::mat4& proj) {
    material.record_draw(cmd_buffer, pipe_layout);
    mesh.record_draw(cmd_buffer, pipe_layout, view, proj);
  }
};