#pragma once

#include "../display_layer/display_layer.h"
#include "entity.h"

struct CameraProjectionData {
  glm::mat4 view;
  glm::mat4 projection;
};

class Camera : public Entity {
 public:
  glm::mat4 entity_to_world =
      glm::lookAt(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, -3.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));

  glm::mat4 projection;

  float fov = 45.f;

  float clip_near = 0.1f;

  float clip_far = 10.0f;

  Camera(Display* display_ptr) : display{display_ptr} {
    update_projection_mat();
  }

  CameraProjectionData get_projection_data() {
    return {entity_to_world, projection};
  }

  virtual void sdl_handle_tick() {}

  virtual void sdl_event_handler(SDL_Event& event) {}

 protected:
  Display* display;

 private:
  void update_projection_mat() {
    auto surface_extend = display->swapchain.get_surface_extend();
    auto proj = glm::perspective(
        glm::radians(fov), surface_extend.width / (float)surface_extend.height,
        clip_near, clip_far);
    proj[1][1] *= -1;
    projection = proj;
  }
};