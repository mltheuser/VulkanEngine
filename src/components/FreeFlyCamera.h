#pragma once

#include "camera.h"

class FreeFlyCamera : public Camera {
 public:
  FreeFlyCamera(Display* display_ptr) : Camera(display_ptr) {}

  void sdl_handle_tick() override {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     currentTime - startTime)
                     .count();
    startTime = std::chrono::high_resolution_clock::now();

    float speed_per_second = 2.0;

    float factor = speed_per_second * time;

    entity_to_world =
        glm::translate(glm::vec3(-xvel * factor, 0, -yvel * factor)) *
        entity_to_world;
  }

  void sdl_event_handler(SDL_Event& event) override {
    switch (event.type) {
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_a) {
          xvel = -1;
        }
        if (event.key.keysym.sym == SDLK_d) {
          xvel = 1;
        }
        if (event.key.keysym.sym == SDLK_w) {
          yvel = -1;
        }
        if (event.key.keysym.sym == SDLK_s) {
          yvel = 1;
        }
        break;  // Add break statement here

      case SDL_KEYUP:
        if (event.key.keysym.sym == SDLK_a) {
          if (xvel < 0) xvel = 0;
        }
        if (event.key.keysym.sym == SDLK_d) {
          if (xvel > 0) xvel = 0;
        }
        if (event.key.keysym.sym == SDLK_w) {
          if (yvel < 0) yvel = 0;
        }
        if (event.key.keysym.sym == SDLK_s) {
          if (yvel > 0) yvel = 0;
        }
        break;  // Add break statement here

      case SDL_MOUSEMOTION:
        float mouse_speed = 0.25;

        // Extract current up and left vectors
        glm::mat3 entity_to_world_3x3(entity_to_world);
        glm::vec3 current_up = entity_to_world_3x3[1];

        entity_to_world =
            glm::rotate(glm::radians((float)event.motion.xrel * mouse_speed),
                        current_up) *
            entity_to_world;

        glm::vec3 current_left = glm::vec3(1, 0, 0);

        entity_to_world =
            glm::rotate(glm::radians((float)event.motion.yrel * mouse_speed),
                        current_left) *
            entity_to_world;

        SDL_WarpMouseInWindow(display->sdl_window.get(), 320, 240);
        break;
    }
  }

 private:
  float xvel = 0;
  float yvel = 0;
};