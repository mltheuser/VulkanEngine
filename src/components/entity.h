#pragma once
#include <glm/mat4x4.hpp>

class Entity {
 public:
  // creates identity mat by default.
  glm::mat4 entity_to_world {1.f};
};